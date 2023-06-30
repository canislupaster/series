#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cmath>

#include <iostream>
#include <sstream>
#include <vector>

struct Audio;

struct AudioStream {
	std::vector<uint16_t> out;
	AudioStream(): out() {}
	virtual void init(Audio& aud) {};
	virtual void step(Audio& aud) = 0;
};

struct Track {
	AudioStream& stream;
	uint32_t amplitude;
	int64_t pan;

	std::function<void(Audio&)> on_ev(uint32_t new_amplitude) {
		return [this, new_amplitude](Audio& aud){amplitude=new_amplitude;};
	}

	std::function<void(Audio&)> off_ev() {
		return [this](Audio& aud){amplitude=0;};
	}
};

struct Event {
	uint64_t t;
	std::function<void(Audio&)> f;
};

struct Audio {
	FILE* proc;
	uint64_t samplerate;
	unsigned channels;

	std::vector<Event> events;
	std::vector<uint16_t> sine_table;

	Audio(uint64_t samplerate, unsigned channels): samplerate(samplerate), channels(channels) {
		proc = popen((std::stringstream() << "ffplay -i - -f u16le -loglevel error -ar " << samplerate << " -ac " << channels).str().c_str(), "w");

		sine_table.reserve(samplerate);
		for (uint64_t i=0; i<samplerate; i++) {
			sine_table.push_back(static_cast<uint16_t>(std::sinf(M_2_PI*static_cast<float>(i)/static_cast<float>(samplerate))*UINT16_MAX));
		}
	}

	void play(AudioStream& in, float dur) {
		uint64_t samples = static_cast<uint64_t>(dur*static_cast<float>(samplerate));

		in.out.clear();
		in.out.resize(channels, 0);
		in.init(*this);

		auto event = events.begin();

		for (uint64_t i=0; i<samples; i++) {
			for (; event!=events.end() && i==event->t; event++) event->f(*this);
			in.step(*this);
			fwrite(in.out.data(), 2, channels, proc);
		}
	}

	~Audio() {
		pclose(proc);
	}
};

struct AudioMix: AudioStream {
	std::vector<std::unique_ptr<Track>> tracks;

	std::unique_ptr<Track> add(AudioStream& stream, uint32_t amplitude, int32_t pan) {
		std::unique_ptr<Track> ptr(new Track{stream, amplitude, pan});
		tracks.push_back(std::move(ptr));
		return ptr;
	}

	void init(Audio& aud) override {
		for (std::unique_ptr<Track>& track: tracks) {
			track->stream.out.clear();
			track->stream.out.resize(aud.channels, 0);
			track->stream.init(aud);
		}
	}

	void step(Audio& aud) override {
		for (unsigned i=0; i<aud.channels; i++) out[i] = 0;

		for (std::unique_ptr<Track>& track: tracks) {
			if (!track->amplitude) continue;
			track->stream.step(aud);

			out[0] += static_cast<uint16_t>((static_cast<uint32_t>(UINT16_MAX-track->pan)*static_cast<uint32_t>(track->stream.out[0]) >> 16) * track->amplitude >> 16);
			out[1] += static_cast<uint16_t>((static_cast<uint32_t>(UINT16_MAX+track->pan)*static_cast<uint32_t>(track->stream.out[1]) >> 16) * track->amplitude >> 16);

			for (unsigned i=0; i<aud.channels; i++) {
				out[i] += static_cast<uint16_t>(static_cast<uint32_t>(track->stream.out[i]) * track->amplitude >> 16);
			}
		}
	}
};

struct Harmonic {
	float freq_mul;
	uint32_t coeff;
};

enum class Waveform {
	Square, Sine, Saw, Triangle
};

uint32_t waveform(Waveform w, Audio& aud, uint64_t t) {
	switch (w) {
		case Waveform::Square: return t % aud.samplerate > aud.samplerate/2 ? UINT16_MAX : 0;
		case Waveform::Sine: return aud.sine_table[t % aud.sine_table.size()];
		case Waveform::Saw: return (UINT16_MAX*(t % aud.samplerate))/aud.samplerate;
		case Waveform::Triangle: {
			uint64_t i = t % 2 * aud.samplerate;
			return i > aud.samplerate ? 2*UINT16_MAX - ((UINT16_MAX*i)/aud.samplerate) : (UINT16_MAX*t)/aud.samplerate;
		}
	}
}

uint64_t note_freq(uint16_t note) {
	return static_cast<uint64_t>(440.0f*std::exp2((static_cast<float>(note) - 49)/12));
}

struct Synth: public AudioStream {
	uint16_t note;
	std::vector<uint64_t> freqs;
	uint64_t phase;
	uint16_t phase_diff;
	uint32_t amplitude;

	std::vector<Harmonic> harmonics;

	Waveform w;

	Synth(Waveform w): AudioStream(), w(w), note(0), phase(0), phase_diff(0), amplitude(UINT16_MAX), harmonics {{1.0, UINT16_MAX}} {}

	void step(Audio& aud) override {
		for (unsigned k=0; k<harmonics.size(); k++) {
			for (unsigned i=0; i<aud.channels; i++) {
				out[i] = static_cast<uint16_t>(((amplitude*harmonics[k].coeff) >> 16)*waveform(w, aud, freqs[k]*(phase + i*phase_diff)) >> 16);
			}
		}

		phase++;
	}

	void set_note(uint16_t new_note) {
		note=new_note;

		uint64_t freq=note_freq(new_note);

		freqs.clear();
		for (unsigned i=0; i<harmonics.size(); i++) {
			freqs.push_back(static_cast<uint64_t>(static_cast<float>(freq)*harmonics[i].freq_mul));
		}
	}

	std::function<void(Audio& aud)> note_ev(uint16_t new_note) {
		return [this, new_note](Audio& aud){set_note(new_note);};
	}
};

struct Noise: public AudioStream {
	static const uint64_t magic = 1lu<<63 | 1lu<<62 | 1lu<<60 | 1lu<<59;
	uint64_t state;

	uint64_t freq;
	uint64_t freq_variance;
	uint64_t phase;

	Waveform w;

	Noise(uint64_t seed=8349192048124985023): state(seed), freq(UINT64_MAX/2), freq_variance(UINT32_MAX), w(Waveform::Sine), phase(0) {}

	void step(Audio& aud) override {
		for (unsigned i=0; i<aud.channels; i++) {
			state = state<<1 | (1-__builtin_parity(state & magic));
			if (state==UINT64_MAX) state=phase*phase; //fall back on a QCG!

			phase += state&(1lu<<63) ? (freq - (freq_variance*(state>>32) >> 32)) : (freq + (freq_variance*(state>>32) >> 32));
			out[i] = waveform(w, aud, phase);
			if (i%300==0) std::cout<<phase<<std::endl;
		}
	}
};

//int main(int argc, char** argv) {
//	Audio aud(44100, 2);
//	Synth s(Waveform::Square);
//	//s.set_note(40);
//
//	aud.events.insert(aud.events.begin(), {{aud.samplerate, s.note_ev(38)}, {2*aud.samplerate, s.note_ev(40)}, {3*aud.samplerate, s.note_ev(43)}, {4*aud.samplerate, s.note_ev(47)}});
//
//	std::unique_ptr<Noise> n(new Noise());
//	n->freq = note_freq(39);
//	n->freq_variance = UINT32_MAX;
//
//	AudioMix mix;
//	mix.add(s, 1<<8, 0);
//	mix.add(*n, 1<<12, 0);
//
//	aud.play(mix, 10);
//
//	return 0;
//}