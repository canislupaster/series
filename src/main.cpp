#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <algorithm>
#include <string_view>
#include <stack>
#include <tgmath.h>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <list>
#include <queue>
#include <numeric>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/val.h>
#include <emscripten/bind.h>

using namespace emscripten;
#endif

#include <gmp.h>
#include <gmpxx.h>

#include "util.hpp"
#include "map.hpp"

//gee i hope not using atomics wont screw things up
thread_local std::shared_ptr<bool> cont;
thread_local int cont_check=0;

size_t numbits(mpz_class const& x) {
	return mpz_sizeinbase(x.get_mpz_t(), 2);
}

struct IrrationalPower: public std::exception {
	char const* what() const noexcept {
		return "complex / irrational power. (it might be rational but it would require irrationals to express)";
	}
};

struct NoConstant: public std::exception {
	char const* what() const noexcept {
		return "constant is required to take to weird powers, u weirdo";
	}
};

struct ZeroPolynomial: public std::exception {
	char const* what() const noexcept {
		return "ur polynomial is zero, which i therefore cannot invert / take to certain powers";
	}
};

struct IntegrateInverse: public std::exception {
	char const* what() const noexcept {
		return "sorry im no risch -- i dont have access to fancy state of the art innovations, like logarithms";
	}
};

struct LogNotOne: public std::exception {
	char const* what() const noexcept {
		return "sorry you can only log(1+ax+bx^2+...), bc i cba to keep special numbers like log(2) if u log(2+x)";
	}
};

struct ExpNotZero: public std::exception {
	char const* what() const noexcept {
		return "sorry you can only exp(0+ax+bx^2+...), bc i cba to keep special numbers like exp(2) if u exp(2+x)";
	}
};

struct ExpectedSomething: public std::exception {
	char const* what() const noexcept {
		return "expected something... cmon man";
	}
};

struct NoImaginary: public std::exception {
	char const* what() const noexcept {
		return "whoops, it seems that i doesn't exist here. maybe it never did to begin with?";
	}
};

struct MissingDep: public std::exception {
	std::string err;
	MissingDep(std::string const& name) {
		std::ostringstream oss;
		oss<<"missing variable named \""<<name<<'"';
		err=oss.str();
	}
	char const* what() const noexcept { return err.c_str(); }
};

std::optional<mpq_class> rat_pow(int a, int b, mpq_class const& c) {
	int g = std::gcd(a,b); a/=g, b/=g;

	if (b%2==0 && c<0) return std::optional<mpq_class>();

	mpq_class cpow;
	int r1 = mpz_root(cpow.get_den_mpz_t(), c.get_den_mpz_t(), std::abs(b));
	int r2 = mpz_root(cpow.get_num_mpz_t(), c.get_num_mpz_t(), std::abs(b));
	if (r1==0 || r2==0)
		return std::optional<mpq_class>();

	mpz_pow_ui(cpow.get_den_mpz_t(), cpow.get_den_mpz_t(), std::abs(a));
	mpz_pow_ui(cpow.get_num_mpz_t(), cpow.get_num_mpz_t(), std::abs(a));

	return cpow;
}

struct FactRat {
	int fact=1;
	mpq_class x;
	mpz_class fact_v=1;

	void extend() {
		mpz_class out_q;
		while (mpz_tdiv_q_ui(out_q.get_mpz_t(), x.get_den_mpz_t(), fact+1) == 0) {
			fact++, x.get_den() = out_q, fact_v*=fact;
		}
	}

	FactRat(int fact, mpq_class x, mpz_class v): fact(fact), x(x), fact_v(v) {}
	FactRat(int x): x(x) {extend();}
	FactRat(mpq_class x): x(x) {extend();}

	template<bool add>
	FactRat add_sub(FactRat const& other) const {
		FactRat const* t = this, *other_p=&other;
		if (other.fact>fact) std::swap(t,other_p);

		if (other_p->fact>1) {
			mpq_class fact_rat;
			mpz_divexact(fact_rat.get_num_mpz_t(), t->fact_v.get_mpz_t(), other_p->fact_v.get_mpz_t());

			fact_rat *= other_p->x;

			if (add) {fact_rat += t->x;} else {
				fact_rat -= t->x;
				if (other.fact>fact) mpq_neg(fact_rat.get_mpq_t(), fact_rat.get_mpq_t());
			}

			fact_rat /= t->fact_v;
			return fact_rat;
		} else {
			FactRat t_c = *t;

			if (add) {t_c.x += t_c.fact_v * other_p->x;} else {
				t_c.x -= t_c.fact_v * other_p->x;
				if (other.fact>fact) mpq_neg(t_c.x.get_mpq_t(), t_c.x.get_mpq_t());
			}

			t_c.extend();
			return t_c;
		}
	}

	template<bool add>
	FactRat operator-(FactRat const& other) const {
		return add_sub<false>(other);
	}

	FactRat operator+(FactRat const& other) const {
		return add_sub<true>(other);
	}

	FactRat operator*(FactRat const& other) const {
		FactRat const* t = this, *other_p=&other;
		if (other.fact>fact) std::swap(t,other_p);

		FactRat out(t->fact, t->x*other_p->x/other_p->fact_v, t->fact_v);
		out.extend();
		return out;
	}

	FactRat operator/(FactRat const& other) const {
		FactRat out = *this;
		out/=other;
		return out;
	}

	FactRat& operator+=(FactRat const& other) {
		*this = *this+other;
		return *this;
	}

	FactRat& operator-=(FactRat const& other) {
		*this = *this-other;
		return *this;
	}

	FactRat& operator*=(FactRat const& other) {
		x*=other.x;
		if (other.fact>fact) {
			x/=fact_v;
			fact=other.fact, fact_v=other.fact_v;
		} else {
			x/=other.fact_v;
		}

		extend();
		return *this;
	}

	FactRat& operator/=(FactRat const& other) {
		if (other.fact==1) {
			x/=other.x;
		} else if (other.fact>=fact) {
			mpz_class fact_rat;
			mpz_divexact(fact_rat.get_mpz_t(), other.fact_v.get_mpz_t(), fact_v.get_mpz_t());

			x*=fact_rat/other.x;
			fact=1, fact_v=1;
		} else {
			mpz_class fact_rat;
			mpz_divexact(fact_rat.get_mpz_t(), fact_v.get_mpz_t(), other.fact_v.get_mpz_t());

			x/=(other.x*fact_rat);
			fact=1, fact_v=1;
		}

		extend();
		return *this;
	}

	operator bool() const {
		return x.operator bool();
	}

	friend std::ostream& operator<<(std::ostream& os, const FactRat& r) {
		if (r.fact==1) os<<r.x;
		else os<<r.x.get_num()<<"/("<<r.x.get_den()<<"*"<<r.fact<<"!)";

		return os;
	}
};

template<class Rat=mpq_class>
struct IRat {
	Rat r, i;

	IRat(Rat r=0, Rat i=0) : r(r), i(i) {}
	IRat(int r) : r(r), i(0) {}

	IRat operator+(IRat const& other) const {
		return IRat(r + other.r, i + other.i);
	}

	IRat operator-(IRat const& other) const {
		return IRat(r - other.r, i - other.i);
	}

	IRat operator-() const {
		return IRat(-r, -i);
	}

	IRat operator*(IRat const& other) const {
		if (i==0 && other.i==0) return IRat(r*other.r,0);

		return IRat(r * other.r - i * other.i, r * other.i + i * other.r);
	}

	IRat operator/(IRat const& other) const {
		if (i==0 && other.i==0) return IRat(r/other.r,0);

		Rat denom = other.r * other.r + other.i * other.i;
		return IRat((r * other.r + i * other.i) / denom, (i * other.r - r * other.i) / denom);
	}

	IRat& operator+=(IRat const& other) {
		r += other.r, i += other.i;
		return *this;
	}

	IRat& operator-=(IRat const& other) {
		r -= other.r, i -= other.i;
		return *this;
	}

	IRat& operator*=(IRat const& other) {
		if (i==0 && other.i==0) {r*=other.r; return *this;}
		*this = *this * other; return *this;
	}

	IRat& operator/=(IRat const& other) {
		if (i==0 && other.i==0) {r/=other.r; return *this;}
		*this = *this / other; return *this;
	}

	operator bool() const {
		return r || i;
	}

	friend std::ostream& operator<<(std::ostream& os, const IRat& ir) {
		if (ir.r && ir.i) {
			os << ir.r << (ir.i > 0 ? "+" : "") << ir.i << "i";
		} else if (ir.r) {
			os << ir.r;
		} else if (ir.i) {
			os << ir.i << "i";
		} else {
			os << "0";
		}
		
		return os;
	}
};

template<class Rat>
struct RPolynomial;

template<class T>
std::ostream& operator<<(std::ostream& os, RPolynomial<T> const& x);

template<class T>
std::optional<mpq_class> to_rat(IRat<T> const& t) {
	if (t.i) return std::optional<mpq_class>();
	else return to_rat(t.r);
}

std::optional<mpq_class> to_rat(FactRat const& t) {
	mpq_class out;
	out.get_num() = t.x.get_num();
	out.get_den() = t.x.get_den()*t.fact_v;
	return out;
}

std::optional<mpq_class> to_rat(mpq_class const& t) {
	return std::make_optional(t);
}

std::optional<mpq_class> to_rat(mpf_class const& t) {
	return mpq_class(t);
}

struct JobInterface {
	bool done=false;
	virtual void run(std::shared_ptr<JobInterface> ptr) = 0;
	virtual ~JobInterface() {}
};

using JobPtr = std::shared_ptr<JobInterface>;

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
using ConstExprPtr = std::shared_ptr<const Expr>;

struct Entry;
using EntryPtr = std::shared_ptr<Entry>;

struct ThreadPool {
	std::mutex m;
	int busy, n;
	std::condition_variable cond_queue;
	std::condition_variable cond_complete;

	std::queue<JobPtr> unstarted;

	void run();

	ThreadPool(int n): busy(n), n(n) {
		for (int i=0; i<n; i++) {
			std::cout<<"spawning thread #"<<i<<"\n";
			std::thread thd(&ThreadPool::run, this);
			thd.detach();
		}
	}
};

ThreadPool pool(std::min(std::thread::hardware_concurrency(), 40u));

template<class Rat>
struct RPolynomial {
	using Vec = std::vector<Rat>;

	Vec coeffs;
	thread_local static std::function<Rat(IRat<mpq_class> x)> ctor;
	int offset;

	thread_local static std::map<int, Vec> free;
	
	void resize(int size) {
		//just in case i add a mpq freelist lmao, probably wont make things any faster tho
		if (size>0) coeffs.resize(size, ctor(0));
		else coeffs.clear();
	}

	RPolynomial(std::initializer_list<Rat> coeffs, int offset=0): coeffs(coeffs), offset(offset) {}

	void get_free(int num_coeffs) {
		if (free.empty()) return;
		auto it = free.lower_bound(num_coeffs);
		if (it==free.end()) it--;

		coeffs.swap(it->second);
		
		free.erase(it);
	}

	RPolynomial(int num_coeffs=0, int offset=0): offset(offset) {
		get_free(num_coeffs);
		
		for (int i=0; i<std::min(num_coeffs, (int)coeffs.size()); i++)
			coeffs[i]=0; //TODO: remove this and streamline initialization...
		resize(num_coeffs);
	}

	RPolynomial(RPolynomial const& other): offset(other.offset) {
		get_free(other.coeffs.size());
		coeffs.assign(other.coeffs.begin(), other.coeffs.end());
	}

	static RPolynomial from(RPolynomial<IRat<mpq_class>> const& other) {
		RPolynomial out({}, other.offset);
		out.get_free(other.coeffs.size());
		
		for (int i=0; i<other.coeffs.size(); i++) {
			if (i>=out.coeffs.size()) out.coeffs.emplace_back(ctor(other.coeffs[i]));
			else out.coeffs[i]=ctor(other.coeffs[i]);
		}

		return out;
	}

	void canonicalize() {
		while (coeffs.size() && !coeffs.back()) {
			coeffs.pop_back();
		}
		
		auto it = coeffs.begin();
		for (; it!=coeffs.end() && !*it; it++, offset++);

		coeffs.erase(coeffs.begin(), it);

		if (coeffs.empty()) offset=0;
	}
	
	void truncate(int len) {
		if (coeffs.size()+offset-1>len) {
			resize(std::max(0,len-offset+1));
			canonicalize();
		}
	}

	template<bool Add>
	void addsub(RPolynomial const& other) {
		int new_offset = std::min(offset, other.offset);

		resize(std::max(coeffs.size()+offset, other.coeffs.size()+other.offset)-new_offset);
		
		if (other.offset<offset) {
			std::rotate(coeffs.begin(), coeffs.begin()+offset-other.offset, coeffs.end());
			offset=other.offset;
		}

		for (size_t i=other.offset-new_offset, i_other=0; i_other<other.coeffs.size(); i++, i_other++) {
			if (Add) coeffs[i]+=other.coeffs[i_other];
			else coeffs[i]-=other.coeffs[i_other];
		}

		canonicalize();
	}

	void operator+=(RPolynomial const& other) {
		addsub<true>(other);
	}

	void operator-=(RPolynomial const& other) {
		addsub<false>(other);
	}

	void negate() {
		for (Rat& x: coeffs) x=-x;
	}

	void mul(RPolynomial const& other, int len) {
		offset += other.offset;
		int i_max = std::min((int)(coeffs.size()+other.coeffs.size()-2), len-offset);

		if (i_max<0) {
			coeffs.clear(), offset=0;
			return;
		}

		resize(i_max+1);

		Rat x;
		for (int i=i_max; i>=0 && *cont; i--) {
			x=ctor(0);

			for (int j=std::max(i-(int)other.coeffs.size()+1, 0); j<=std::min(i,(int)coeffs.size()-1); j++) {
				x+=coeffs[j]*other.coeffs[i-j];
			}

			coeffs[i]=x;
		}

		canonicalize();
	}

	void karatsuba_mul(RPolynomial const& other, int len) {
		if (!*cont) return;
			
		if (coeffs.size()*other.coeffs.size()<100000) {
			mul(other, len);
			return;
		}

		int mid = std::min(coeffs.size(), other.coeffs.size())/2;

		RPolynomial a(mid,offset);
		a.coeffs.assign(std::make_move_iterator(coeffs.begin()), std::make_move_iterator(coeffs.begin()+mid));

		a.canonicalize();
		coeffs.erase(coeffs.begin(), coeffs.begin()+mid);
		canonicalize();

		RPolynomial c(mid,other.offset),d(other.coeffs.size()-mid,other.offset);
		c.coeffs.assign(other.coeffs.begin(), other.coeffs.begin()+mid);
		d.coeffs.assign(other.coeffs.begin()+mid, other.coeffs.end());

		RPolynomial e = a;
		e+=*this;

		{
			RPolynomial f=c;
			f+=d;
			e.karatsuba_mul(f, len-mid);
		}

		a.karatsuba_mul(c, len); karatsuba_mul(d, len-mid);

		e-=*this;

		offset += 2*mid;
		truncate(len);
		*this+=a;

		a.truncate(len-mid);
		e-=a;
		e.offset += mid;

		*this+=e;
		canonicalize();
	}

	RPolynomial inv(int len) const {
		if (coeffs.empty()) throw ZeroPolynomial();

		len += offset;

		RPolynomial out(len+1,-offset);
		RPolynomial res(len+1);

		for (int i=0; i<=len && *cont; i++) {
			if (i==0) out.coeffs[i] = ctor(1);
			else out.coeffs[i] = -res.coeffs[i];
			out.coeffs[i] /= coeffs[0];

			for (int j=i+1; j<std::min(len, (int)coeffs.size()+i); j++) {
				res.coeffs[j] += out.coeffs[i]*coeffs[j-i];
			}
		}

		out.canonicalize();
		return out;
	}

	template<class F>
	void mul_range(F done, RPolynomial& base, RPolynomial const& inc, int num, int len) const;

	RPolynomial substitute(RPolynomial const& other, int len) const {
		int off = std::max(other.offset,0);

		RPolynomial out(0, std::min(offset*other.offset, (other.offset+(int)other.coeffs.size()-1)*offset));

		auto addmul = [&out](RPolynomial const& other, Rat mult) {
			int out_off = other.offset-out.offset;
			if (out_off+(int)other.coeffs.size()>(int)out.coeffs.size())
				out.resize(out_off+other.coeffs.size());
			for (int i=0, i_other=out_off; i<other.coeffs.size(); i++, i_other++)
				out.coeffs[i_other]+=mult*other.coeffs[i];
		};

		if (other.offset<0) {
			int start = std::min(-1, other.offset+(int)other.coeffs.size()-1);

			if (offset>=0) {
				RPolynomial invthis = positive_pow(-other.offset, len+2*offset).inv(len);
				RPolynomial invpow = invthis;

				mul_range([&](RPolynomial& x, int i) {
					addmul(x,other.coeffs[i]);
				}, invpow, *this, start-other.offset+1, len);
			} else {
				int inv_len = len-(start+1)*offset;
				RPolynomial invthis = inv(inv_len);
				RPolynomial invpow = invthis.positive_pow(-start, len);

				mul_range([&](RPolynomial& x, int i) {
					addmul(x,other.coeffs[start-i]);
				}, invpow, invthis, start-other.offset+1, len);
			}
		}

		if (other.offset+other.coeffs.size()-1>=0) {
			int begin = std::max(0,-other.offset);

			if (offset>=0) {
				RPolynomial t_pow = positive_pow(off, len);

				mul_range([&](RPolynomial& x, int i) {
					addmul(x,other.coeffs[i]);
				}, t_pow, *this, other.coeffs.size()-begin, len);
			} else {
				RPolynomial t_pow = positive_pow(other.offset+other.coeffs.size()-1, len);
				RPolynomial invthis = inv(len+offset);

				mul_range([&](RPolynomial& x, int i) {
					addmul(x,other.coeffs[other.coeffs.size()-begin-1-i]);
				}, t_pow, invthis, other.coeffs.size()-begin, len);
			}
		}

		out.canonicalize();
		return out;
	}

	RPolynomial substitute_skip(RPolynomial const& other, int len) const {
		RPolynomial& x = const_cast<RPolynomial&>(*this); //🧌

		Rat constant = coeffs[0];

		auto it = x.coeffs.begin();
		for (; it!=x.coeffs.end() && (x.offset==0 || !*it); x.offset++, it++);
		x.coeffs.erase(x.coeffs.begin(), it);

		RPolynomial out = x.substitute(other, len);
		
		for (; x.offset>0; x.offset--) {
			x.coeffs.insert(x.coeffs.begin(), x.offset==1 ? constant : ctor(0));
		}

		return out;
	}

	RPolynomial positive_pow(int pow, int len) const {
		if (pow==0) {
			return RPolynomial({ctor(1)}, 0);
		} else if (pow==1) {
			return *this;
		}

		assert(pow>0);

		RPolynomial out({ctor(1)}, 0);
		RPolynomial tmp = *this;

		for (; pow>0 && *cont; pow>>=1) {
			if (pow&1) {
				out.mul(tmp, len);
			}

			RPolynomial tmp_cpy = tmp;
			tmp.mul(tmp_cpy, len);
		}

		out.canonicalize();
		return out;
	}

	static RPolynomial binom_series(int len, int a, int b, Rat c) {
		RPolynomial series(len+1);

		std::optional<mpq_class> c_rat_opt = to_rat(c);
		if (!c_rat_opt) throw IrrationalPower();
		std::optional<mpq_class> cpow_opt = rat_pow(a,b,*c_rat_opt);
		if (!cpow_opt) throw IrrationalPower(); //...

		mpq_class cpow=*cpow_opt, c_rat=*c_rat_opt;

		if (std::signbit(a)!=std::signbit(b)) {
			cpow=1/cpow;
		}

		series.coeffs[0] = ctor(cpow);
		mpq_class mul = cpow;

		mpq_class k = mpq_class(mpz_class(a), mpz_class(b));
		for (int i=1; i<=len && *cont; i++) {
			mul *= k/(i*c_rat);
			series.coeffs[i] = ctor(mul);
			
			k-=1;
			if (k==0) break;
		}

		return series;
	}

	static RPolynomial exp_series(int len, Rat exp) {
		RPolynomial out(len+1);

		Rat mul=ctor(1);
		for (int i=0; i<=len && *cont; i++) {
			out.coeffs[i]=mul;
			mul*=exp/ctor(i+1);
		}

		return out;
	}

	static RPolynomial ln_series(int len, Rat x) {
		RPolynomial series(len+1, 1);

		Rat mul=ctor(1);
		for (int i=0; i<=len && *cont; i++) {
			mul*=x;
			if (i%2==1) series.coeffs[i]=-mul/ctor(i+1);
			else series.coeffs[i]=mul/ctor(i+1);
		}

		return series;
	}

	static RPolynomial sin_cos_series(int len, bool sin, Rat x) {
		RPolynomial out(len+1, sin ? 1 : 0);

		Rat mul = x;

		x*=x;
		for (int i=sin ? 1 : 0, term=0; term<=len && *cont; i+=2, term+=2) {
			out.coeffs[term] = mul;
			mul *= -x/ctor((i+1)*(i+2));
		}

		return out;
	}

	bool operator==(RPolynomial const& other) const {
		return coeffs==other.coeffs && offset==other.offset;
	}

	RPolynomial pow(int len, int a, int b) const {
		if (a==0) return RPolynomial({ctor(1)});
		if (a>0 && b>0 && (b==1 || a%b==0)) return positive_pow(a/b, len);

		if (coeffs.empty()) {
			if (a*b>0) return RPolynomial();
			throw ZeroPolynomial();
		}

		if (offset!=0) throw NoConstant();
		RPolynomial series = binom_series(len, a,b, coeffs[0]);

		return substitute_skip(series, len);
	}

	RPolynomial log(int len, Rat mul=ctor(1)) const {
		if (offset!=0 || coeffs.empty() || coeffs[0]!=1) throw LogNotOne();

		RPolynomial series = ln_series(len, mul);
		return substitute_skip(series, len);
	}

	void derivative() {
		if (offset==0) coeffs.erase(coeffs.begin());
		else {
			if (offset<0) {
				if (offset+coeffs.size()==1) coeffs.pop_back();
				else if (offset+coeffs.size()>1) coeffs[-offset]=0;
			}

			offset--;
		}

		for (int i=0, j=offset+1; i<coeffs.size(); i++, j++) {
			coeffs[i] *= j;
		}
	}

	void integral() {
		if (offset<0 && offset+coeffs.size()-1>=-1 && coeffs[-offset-1]) throw IntegrateInverse();

		offset++;
		for (int i=0, j=offset; i<coeffs.size(); i++, j++) {
			coeffs[i] /= j;
		}
	}

	RPolynomial exp(int len, Rat mul=ctor(1)) const {
		if (offset<=0) throw ExpNotZero();

		RPolynomial series = exp_series(len, mul);
		return substitute(series, len);
	}

	RPolynomial sin_cos(size_t len, bool sin, Rat mul=ctor(1)) const {
		if (offset<=0) throw ExpNotZero();

		RPolynomial series = sin_cos_series(len , sin, mul);
		return substitute(series, len);
	}

	friend std::ostream& operator<<(std::ostream& os, RPolynomial const& x) {
		bool added=false;

		for (int i=0; i<x.coeffs.size(); i++) {
			if (!x.coeffs[i]) continue;

			os<<(added ? " + " : "");

			os<<"("<<x.coeffs[i]<<")"<<"x^"<<i+x.offset;

			added=true;
		}

		return os;
	}

	RPolynomial(RPolynomial&& other): coeffs(std::move(other.coeffs)), offset(other.offset) {}
	RPolynomial& operator=(RPolynomial&& other) {coeffs.swap(other.coeffs); offset=other.offset; return *this;}
	RPolynomial& operator=(RPolynomial const& other) {coeffs=other.coeffs; offset=other.offset; return *this;}

	~RPolynomial() {
		if (coeffs.size()>100) free.emplace(coeffs.size(), std::move(coeffs));
	}
};

template<class T>
thread_local std::map<int, std::vector<T>> RPolynomial<T>::free = {};

template<class T>
thread_local std::function<T(IRat<mpq_class>)> RPolynomial<T>::ctor = [](IRat<mpq_class> x){
	throw std::runtime_error("whoops i forgot to initialize the constructor, which happens to be stored in a global variable (aka very a safe efficient and idiomatic way to keep very temporary and instance-specific data)");
	return T(x);
};

template<class R>
struct MulJob: JobInterface {
	std::function<R(IRat<mpq_class>)>	ctor;

	RPolynomial<R> x;
	std::shared_ptr<std::vector<RPolynomial<R>>> incs;
	int len;

	int i, step, num;

	std::shared_ptr<bool> cont_ptr;
	std::shared_ptr<std::vector<JobPtr>> completed;

	MulJob(std::function<R(IRat<mpq_class>)> ctor, RPolynomial<R> x, std::shared_ptr<std::vector<RPolynomial<R>>> incs, int len, int i, int step, int num, std::shared_ptr<bool> cont_ptr, std::shared_ptr<std::vector<JobPtr>> completed): ctor(ctor), x(x), incs(incs), len(len), i(i), step(step), num(num), cont_ptr(cont_ptr), completed(completed) {}
	
	void run(JobPtr ptr) {
		cont=cont_ptr;
		RPolynomial<R>::ctor = ctor;

		x.karatsuba_mul(incs->at(step), len);

		int num_job=0;

		{
			std::lock_guard lock(pool.m);

			if (*cont) {
				completed->push_back(ptr);

				for (int j=0; j<step && i+(1<<j)<num; j++, num_job++) {
					auto newjob = std::make_shared<MulJob<R>>(*this);
					newjob->i=i+(1<<j), newjob->step=j;

					pool.unstarted.push(newjob);
				}
			}
		}

		pool.cond_complete.notify_one();
		for (int i=1; i<num_job; i++) pool.cond_queue.notify_one();
	}
};

template<class Rat>
template<class F>
void RPolynomial<Rat>::mul_range(F done, RPolynomial& base, RPolynomial const& inc, int num, int len) const {
	done(base, 0);

	if (static_cast<size_t>(num)*(size_t)(len-base.offset)*(size_t)(len-inc.offset) < 1e6) {
		for (int i=1; i<num; i++) {
			base.karatsuba_mul(inc, len);
			done(base, i);
		}

		return;
	}

	int logn=0; for (; (1<<(logn+1))<=num; logn++);
	auto incs = std::make_shared<std::vector<RPolynomial>>(logn+1);
	auto completed = std::make_shared<std::vector<JobPtr>>();

	incs->front() = std::move(inc);
	for (int i=0; i<=logn && *cont; i++) {
		if (i>0) {
			RPolynomial& x = incs->at(i), &y = incs->at(i-1);

			x=y;
			x.karatsuba_mul(y, len);
		}

		{
			std::lock_guard lock(pool.m);
			pool.unstarted.push(std::make_shared<MulJob<Rat>>(ctor, base, incs, len, (1<<i), i, num, cont, completed));
		}

		pool.cond_queue.notify_one();
	}

	std::unique_lock lock(pool.m);
	for (int i=1; i<num && *cont; ) {
		while (completed->empty() && *cont) {
			pool.cond_complete.wait(lock);
		}

		while (completed->size() && *cont) {
			JobPtr ptr = completed->back();
			auto j = static_cast<MulJob<Rat>&>(*completed->back());
			completed->pop_back();

			lock.unlock();
			done(j.x, j.i);
			lock.lock();
			i++;
		}
	}
}

//
//struct MVRPolynomial {
//	std::variant<std::vector<MVRPolynomial>, RPolynomial> x;
//
//	void operator+=(MVRPolynomial const& other) {
//		assert(x.index()==other.x.index());
//		RPolynomial* poly = std::get_if<RPolynomial>(&x);
//
//		if (poly) {
//		} else {
//			std::vector<MVRPolynomial>& a = std::get<std::vector<MVRPolynomial>>(x);
//			std::vector<MVRPolynomial>& b = std::get<std::vector<MVRPolynomial>>(other.x);
//
//			if (a.empty() && b.empty()) return;
//
//			bool sub_mv = x.size() ? x[0].mv : other.x[0].mv;
//			x.resize(std::max(x.size(), other.x.size()), sub_mv ? MVRPolynomial() : MVRPolynomial(RPolynomial {.coeffs={}, .den=1}));
//
//			for (size_t i=0; i<x.size(); i++) {
//				x[i]+=other.x[i];
//			}
//		} else {
//			y+=other.y;
//		}
//
//	}
//
//	~MVRPolynomial() {
//		if (mv) x.~vector<MVRPolynomial>();
//		else y.~RPolynomial();
//	}
//};

enum Precedence: int {
	none=-1,
	addsub=1,
	muldiv=2,
	substitute=3,
	minus=4,
	pow=5,
	literal=6,
	nobind=7
};

bool starts_with(const char** x, char const* cmp) {
	if (strlen(*x)>=strlen(cmp) && strncmp(*x,cmp,strlen(cmp))==0) {
		(*x)+=strlen(cmp);
		return true;
	}

	return false;
}

void skip_ws(const char** x) {
	while (iswspace(**x)) (*x)++;
}

struct Notebook;

struct Function {
	enum Ty {
		Exp,
		Log,
		Sin,
		Cos,
		Sqrt,
		Cbrt,
		Negate,
		Differentiate,
		Integrate
	};

	Ty ty;
	ExprPtr arg;
};

struct Op {
	enum class Ty {
		Add,Sub,Div,Mul,Pow,Substitute
	};

	Ty ty;

	ExprPtr x;
	ExprPtr y;
};

struct Var {
	std::string name;
	std::weak_ptr<Entry> ent;
};

struct Expr {
	using V = std::variant<mpq_class, Op, Function, Var>;

	V v;
	Precedence prec;
	
	template<class T>
	Expr(T v, Precedence prec=Precedence::literal): v(v), prec(prec) {}

	explicit Expr() {}
	
	int offset;
	std::optional<mpq_class> rat;

	void set_offset();
	void is_rat();

	Map<std::string, std::monostate> deps() const;
	void link(Notebook& nb);

	void tex(std::ostream& os, Precedence prec=Precedence::none) const;
};

ExprPtr parse(const char** str, Precedence prec) {
	ExprPtr left;

	auto set_func = [&](Function::Ty ty, Precedence prec) {
		left = std::make_shared<Expr>(Function {.ty=ty, .arg=parse(str, prec)}, prec==Precedence::none ? Precedence::nobind : prec);
	};

	auto set_op = [&](Op::Ty ty, Precedence prec) {
		left = std::make_shared<Expr>(Op {.ty=ty, .x=left, .y=parse(str, prec)}, prec);
	};

	skip_ws(str);
	const char* begin=*str;
	if (starts_with(str, "-")) {
		set_func(Function::Negate, Precedence::minus);
	} else if (starts_with(str, "e^")) {
		set_func(Function::Exp, Precedence::pow);
	} else if (starts_with(str, "exp(")) {
		set_func(Function::Exp, Precedence::none);
	} else if (starts_with(str, "sqrt(")) {
		set_func(Function::Sqrt, Precedence::none);
	} else if (starts_with(str, "cbrt(")) {
		set_func(Function::Cbrt, Precedence::none);
	} else if (starts_with(str, "ln(")) {
		set_func(Function::Log, Precedence::none);
	} else if (starts_with(str, "sin(")) {
		set_func(Function::Sin, Precedence::none);
	} else if (starts_with(str, "cos(")) {
		set_func(Function::Cos, Precedence::none);
	} else if (starts_with(str, "d")) {
		set_func(Function::Differentiate, Precedence::muldiv);
	} else if (starts_with(str, "int")) {
		set_func(Function::Integrate, Precedence::muldiv);
	} else if (starts_with(str, "(")) {
		left=parse(str, Precedence::none);
	} else if (isalpha(**str)) {
		while (isalnum(**str)) (*str)++;
		std::string name(begin, *str);
		left = std::make_shared<Expr>(Var {.name=name});
	} else {
		while (isdigit(**str) || **str=='e' || **str=='.') (*str)++;

		left=std::make_shared<Expr>(mpq_class());
		std::get<mpq_class>(left->v).set_str(std::string(begin, *str), 10);
		if (*str==begin) throw ExpectedSomething();
	}

	while (true) {
		skip_ws(str);
		if (prec<Precedence::addsub && starts_with(str, "+")) {
			set_op(Op::Ty::Add, Precedence::addsub);
		} else if (prec<Precedence::addsub && starts_with(str, "-")) {
			set_op(Op::Ty::Sub, Precedence::addsub);
		} else if (prec<Precedence::muldiv && starts_with(str, "/")) {
			set_op(Op::Ty::Div, Precedence::muldiv);
		} else if (prec<Precedence::muldiv && starts_with(str, "*")) {
			set_op(Op::Ty::Mul, Precedence::muldiv);
		} else if (prec<Precedence::substitute && starts_with(str, "(")) {
			left = std::make_shared<Expr>(Op {.ty=Op::Ty::Substitute, .x=left, .y=parse(str, Precedence::none)}, left->prec);
		} else if (prec<Precedence::pow && starts_with(str, "^")) {
			set_op(Op::Ty::Pow, Precedence::pow);
		} else if (prec==Precedence::none) {
			starts_with(str, ")");
			break;
		} else {
			break;
		}
	}

	return left;
}

//you might be thinking: why the hell is this so gargantuan?
//valid. but i may multithread regular ass operations
template<class R>
struct EvalJob: JobInterface {
	std::function<R(IRat<mpq_class>)> ctor;
	RPolynomial<R> out;

	ConstExprPtr to_eval;
	int len;
	std::shared_ptr<bool> cont_ptr;

	std::weak_ptr<Entry> ent;
	std::shared_ptr<std::mutex> nb_lock;

	std::optional<std::string> err;

	EvalJob(Entry& ent, std::weak_ptr<Entry> ent_p, std::function<R(IRat<mpq_class>)> ctor);

	void run(std::shared_ptr<JobInterface> ptr);
	RPolynomial<R> gf(ConstExprPtr ex, int len) const;
};

struct EntryOutputInterface {
	virtual int get_offset() const = 0;
	virtual std::string get_output() const = 0;
	virtual RPolynomial<IRat<mpq_class>> get_poly() const = 0;
	virtual std::optional<IRat<mpq_class>> is_constant() const = 0;

	virtual ~EntryOutputInterface() {}
};

template<class R>
struct EntryOutput: EntryOutputInterface {
	RPolynomial<R> output;

	EntryOutput(RPolynomial<R> output): output(output) {}

	int get_offset() const {return output.offset;}

	std::string get_output() const {static_assert(False<R>::value);}
	RPolynomial<IRat<mpq_class>> get_poly() const {
		RPolynomial<IRat<mpq_class>> out;
		out.offset=output.offset;
		for (int i=0; i<output.coeffs.size(); i++) {
			out.coeffs.emplace_back(*to_rat(output.coeffs[i].r), *to_rat(output.coeffs[i].i));
		}

		return out;
	}

	std::optional<IRat<mpq_class>> is_constant() const {
		if (output.coeffs.size()>1 || output.offset!=0) return std::optional<IRat<mpq_class>>();
		return std::make_optional<IRat<mpq_class>>(*to_rat(output.coeffs[0].r), *to_rat(output.coeffs[0].i));
	}
};

template<>
RPolynomial<IRat<mpq_class>> EntryOutput<IRat<mpq_class>>::get_poly() const {
	return output;
}

template<>
std::string EntryOutput<IRat<mpq_class>>::get_output() const {
	std::ostringstream oss;
	oss<<output.offset<<"\n";

	for (int i=0; i<output.coeffs.size(); i++) {
		oss<<output.coeffs[i].r.get_num()<<"\n"<<output.coeffs[i].r.get_den()<<"\n";
		oss<<output.coeffs[i].i.get_num()<<"\n"<<output.coeffs[i].i.get_den()<<"\n";
	}

	return oss.str();
}

template<>
std::string EntryOutput<IRat<FactRat>>::get_output() const {
	std::ostringstream oss;
	oss<<output.offset<<"\n";

	mpz_class fact_start;
	mpz_fac_ui(fact_start.get_mpz_t(), std::abs(output.offset-1));
	if (output.offset<0 && output.offset%2==1)
		mpz_neg(fact_start.get_mpz_t(), fact_start.get_mpz_t());

	int j=output.offset;
	mpz_class d; mpq_class q;

	auto format = [&oss,&j,&fact_start,d,q] (FactRat const& x) mutable {
		q=x.x;
		if (x.fact>std::abs(j)) {
			mpz_divexact(d.get_mpz_t(), x.fact_v.get_mpz_t(), fact_start.get_mpz_t());
			q/=d;
		} else {
			mpz_divexact(d.get_mpz_t(), fact_start.get_mpz_t(), x.fact_v.get_mpz_t());
			q*=d;
		}

		oss<<q.get_num()<<"\n"<<q.get_den()<<"\n";
	};

	for (int i=0; i<output.coeffs.size(); i++,j++) {
		if (j<0) mpz_divexact_ui(fact_start.get_mpz_t(), fact_start.get_mpz_t(), 1-j);
		else if (j>0) fact_start *= j;

		format(output.coeffs[i].r);
		format(output.coeffs[i].i);
	}

	oss<<"\n";

	return oss.str();
}

template<>
std::string EntryOutput<IRat<mpf_class>>::get_output() const {
	std::ostringstream oss;
	oss<<output.offset<<"\n";

	auto format = [&oss](mpf_class const& x) {
		mp_exp_t e;
		std::string s = x.get_str(e);
		e = e-s.length();
		oss<<s<<"\n"<<e<<"\n";
	};

	for (int i=0; i<output.coeffs.size(); i++) {
		format(output.coeffs[i].r);
		format(output.coeffs[i].i);
	}

	return oss.str();
}

struct Entry {
	std::string name;
	std::shared_ptr<std::mutex> nb_lock;

	ConstExprPtr ex;
	std::unique_ptr<EntryOutputInterface> output;
	int len;

	int mark;
	int cycle_mark;

	std::optional<std::string> parse_error;
	std::optional<std::string> eval_error;

	//unfortunately used as indicator for change, even though we already have a variable for that...
	bool cycle;
	std::optional<MissingDep> missing_dep;
	std::vector<EntryPtr> dependents;
	Map<std::string, std::monostate> deps;

	enum class OutputKind {
		Float, EGF, OGF
	} out_kind;

	int float_prec;

	JobPtr job;
	bool changed=false;
	std::shared_ptr<bool> eval_cont;

	static void evaluate(EntryPtr ent, std::unique_lock<std::mutex>& lock) {
		std::cout<<"evaluating "<<ent->name<<std::endl;

		ent->cancel();
		ent->eval_cont = std::make_shared<bool>(true);

		ent->eval_error.reset();

		std::cout<<"spawning job"<<std::endl;

		switch (ent->out_kind) {
			case OutputKind::OGF: {
				ent->job = std::make_shared<EvalJob<IRat<mpq_class>>>(*ent, ent, [](IRat<mpq_class> x){
						return IRat<mpq_class>(x);
					});

				break;
			}
			case OutputKind::Float: {
				int fp = ent->float_prec;
				ent->job = std::make_shared<EvalJob<IRat<mpf_class>>>(*ent, ent, [fp](IRat<mpq_class> x){
						return IRat<mpf_class>(mpf_class(x.r, fp), mpf_class(x.i, fp));
					});
					
				break;
			}
			case OutputKind::EGF: {
				ent->job = std::make_shared<EvalJob<IRat<FactRat>>>(*ent, ent, [](IRat<mpq_class> x){
						return IRat<FactRat>(FactRat(x.r), FactRat(x.i));
					});

				break;
			}
		}

		bool b;

		{
			std::lock_guard pool_lock(pool.m);

			b = pool.busy==pool.n;
			std::cout<<pool.busy<<" "<<pool.n<<std::endl;
			if (!b) pool.unstarted.push(ent->job);
		}

		if (b) {
			std::cout<<"pool busy, launching on main thread"<<std::endl;
			lock.unlock();
			ent->job->run(ent->job);
			lock.lock();
		} else {
			pool.cond_queue.notify_one();
		}
	}

	void propagate(std::unique_lock<std::mutex>& lock) {
		for (auto dep: dependents) {
			std::cout<<name<<" (done evaluating) dependend on by "<<dep->name<<" "<<dep->mark<<" "<<dep->deps.count<<std::endl;
			if (!dep->cycle && !dep->missing_dep && (++dep->mark)==dep->deps.count) {
				evaluate(dep, lock);
			}
		}
	}

	void cancel() {
		if (eval_cont) *eval_cont = false;
		job.reset();
	}

	void wait() {
		JobPtr p;
		{
			std::lock_guard lock(*nb_lock);
			if (!job) return;
			p=job;
		}

		std::unique_lock lock(pool.m);
		while (!p->done) {
			pool.cond_complete.wait(lock);
		}
	}

	enum class Status {
		Loading,Updated,NoChange
	};

	Status status() {
		std::lock_guard lock(*nb_lock);
		if (job || (mark!=-1 && mark<deps.count)) return Status::Loading;
		else if (changed) {changed=false; return Status::Updated;}
		else return Status::NoChange;
	}

	std::string get_output() {
		std::lock_guard lock(*nb_lock);
		return output->get_output();
	}

	std::string get_error() {
		std::lock_guard lock(*nb_lock);

		if (parse_error) return *parse_error;
		else if (missing_dep) return missing_dep->what();
		else if (cycle) return "dependency cycle";
		else if (eval_error) return *eval_error;
		else return "";
	}

	std::string tex_input() {
		std::ostringstream oss;
		if (ex) ex->tex(oss);
		return oss.str();
	}

	void link(Notebook& nb) {
		try {
			//...
			ExprPtr new_ex = std::make_shared<Expr>(*ex);
			new_ex->link(nb);
			ex = new_ex;

			missing_dep.reset();
		} catch (MissingDep d) {
			missing_dep=d;
		}
	}

	void parse(std::string const& value) {
		const char* cstr = value.c_str();

		try {
			ExprPtr new_ex = ::parse(&cstr, Precedence::none);
			if (strlen(cstr)) throw std::runtime_error("unexpected stuff while parsing");

			new_ex->is_rat();
			new_ex->set_offset();
			ex=new_ex;

			parse_error.reset();
		} catch (std::exception const& e) {
			ex.reset();
			parse_error=e.what();
		}
	}
};

template<class R>
void EvalJob<R>::run(JobPtr ptr) {
	cont = cont_ptr;
	RPolynomial<R>::ctor = ctor;

	try {
		out = gf(to_eval, len);
	} catch (std::exception const& ex) {
		err.emplace(ex.what());
	}

	std::unique_lock lock(*nb_lock);
	if (*cont) {
		if (!ent.expired()) {
			EntryPtr p = ent.lock();

			if (err) p->eval_error.emplace(*err);
			else p->output.reset(new EntryOutput<R>(std::move(out)));

			std::cout<<"resetting job for "<<p->name<<std::endl;
			p->job.reset();
			p->eval_cont.reset();
			p->changed=true;

			p->propagate(lock);
		}
	}
}

template<class R>
EvalJob<R>::EvalJob(Entry& ent, std::weak_ptr<Entry> ent_p, std::function<R(IRat<mpq_class>)> ctor):
	ctor(ctor), to_eval(ent.ex), len(ent.len), cont_ptr(ent.eval_cont), ent(ent_p), nb_lock(ent.nb_lock) {}

struct Notebook {
	std::vector<EntryPtr> entries;
	Map<std::string_view, EntryPtr, true> names;

	std::shared_ptr<std::mutex> nb_lock;

	Notebook(): nb_lock(std::make_shared<std::mutex>()) {}

	EntryPtr add_entry(std::string name, std::string value, int len, Entry::OutputKind outkind, int float_prec);
	void remove_entry(EntryPtr ent);
	void update_entry(EntryPtr ent, std::string value, int len, Entry::OutputKind outkind, int float_prec);
	void rename_entry(EntryPtr ent, std::string new_name);

	void check(std::unique_lock<std::mutex>& lock);

	~Notebook() {
		std::lock_guard lock(*nb_lock);
		for (EntryPtr ent: entries) ent->cancel();
	}
};

void Expr::tex(std::ostream& os, Precedence min_prec) const {
	if (prec<min_prec) os<<"\\left(";
	Precedence in_prec = prec;

	std::visit(overloaded {
		[&](mpq_class const& q) {os<<q;},
		[&](Op const& o) {
			if (o.ty==Op::Ty::Div) {
				os<<"\\frac{"; o.x->tex(os, in_prec); os<<"}{"; o.y->tex(os, in_prec); os<<"}";
				return;
			}

			os<<"{"; o.x->tex(os, in_prec); os<<"}";

			switch (o.ty) {
				case Op::Ty::Pow: os<<"^"; break;
				case Op::Ty::Sub: os<<" - "; break;
				case Op::Ty::Add: os<<" + "; break;
				case Op::Ty::Mul: os<<" \\cdot "; break;
				case Op::Ty::Substitute: in_prec=Precedence::nobind; break;
				default:;
			}

			os<<"{"; o.y->tex(os, in_prec); os<<"}";
		},
		[&](Function const& f) {
			switch (f.ty) {
				case Function::Ty::Exp: os<<"e^{"; break;
				case Function::Ty::Cos: os<<"\\cos {"; break;
				case Function::Ty::Sin: os<<"\\sin {"; break;
				case Function::Ty::Log: os<<"\\ln {"; break;
				case Function::Ty::Sqrt: os<<"\\sqrt {", in_prec=Precedence::none; break;
				case Function::Ty::Cbrt: os<<"\\cbrt {", in_prec=Precedence::none; break;
				case Function::Ty::Differentiate: {
					os<<"\\frac{d"; f.arg->tex(os, in_prec);
					os<<"}{dx}";
					return;
				}
				case Function::Ty::Integrate: {
					os<<"\\int_{0}^{x} {";
					f.arg->tex(os, in_prec);
					os<<"} \\,dx";
					return;
				}
				case Function::Ty::Negate: os<<"-"; break;
			}

			f.arg->tex(os, in_prec);
			if (f.ty!=Function::Ty::Negate) os<<"}";
		},
		[&](Var const& x) {os<<x.name;}
	}, v);

	if (prec<min_prec) os<<"\\right)";
}

bool ratint(mpq_class const& rat) {
	return rat.get_num().fits_sint_p() && rat.get_den().fits_sint_p();
}

void Expr::set_offset() {
	offset = std::visit(overloaded {
			[&](mpq_class& q) {
				return 0;
			},
			[&](Op& o) {
				o.x->set_offset();
				o.y->set_offset();
				
				switch (o.ty) {
					case Op::Ty::Div: return o.x->offset - o.y->offset;
					case Op::Ty::Pow: {
						if (o.y->rat && ratint(*o.y->rat)) {
							if (rat->get_den()==1) {
								int mul = static_cast<int>(o.y->rat->get_num().get_si());
								return mul*o.x->offset;
							}
						}

						return 0;
					}
					case Op::Ty::Add:
					case Op::Ty::Sub: {
						//not really accurate if everything cancels... oh well.
						//i dont think this is solvable without creating a way to get the next term, but that would be hella painful. alternatively, binary search (but will probably be much faster since we have lower bound) until u get nonzero term / run outta space

						return std::min(o.x->offset, o.y->offset);
					}
					case Op::Ty::Mul:
						return o.x->offset + o.y->offset;
					case Op::Ty::Substitute:
						//doesn't really work when one is negative. unfortunately 🤷‍
						return o.x->offset*o.y->offset;
				}
			},
			[&](Function& f) {
				f.arg->set_offset();

				switch (f.ty) {
					case Function::Ty::Negate:
					case Function::Ty::Sin:
					case Function::Ty::Log:
						return f.arg->offset;
					case Function::Ty::Sqrt: return f.arg->offset/2;
					case Function::Ty::Cbrt: return f.arg->offset/3;
					case Function::Ty::Integrate:
						return f.arg->offset+1;
					case Function::Ty::Differentiate:
						return f.arg->offset-1;
					default: return 0;
				}
			},
			[&](Var& x) {
				if (!x.ent.expired()) return x.ent.lock()->output->get_offset();
				if (x.name=="i") return 0;
				else return 1;
			}
	}, v);
}

template<class R>
RPolynomial<R> EvalJob<R>::gf(ConstExprPtr ex, int len) const {
	if (!*cont) return RPolynomial<R>();

	return std::visit(overloaded {
		[&](mpq_class const& q) {
			return len>=0 ? RPolynomial<R>({ctor(q)}) : RPolynomial<R>();
		},
		[&](Op const& o) {
			switch (o.ty) {
				case Op::Ty::Div: {
					RPolynomial num = gf(o.x, len+o.y->offset);
					RPolynomial den = gf(o.y, len-num.offset+2*o.y->offset);

					if (den.offset!=o.y->offset) {
						std::cout<<"recomputing values for ur screwed up division operation. sorry! kinda bad at this"<<std::endl;
						num = gf(o.x, len+den.offset);
						den = gf(o.y, len-num.offset+2*den.offset);
					}

					num.mul(den.inv(len-num.offset), len);
					return num;
				}
				case Op::Ty::Pow: {
					if (o.y->rat && ratint(*o.y->rat)) {
						return gf(o.x, len).pow(len, o.y->rat->get_num().get_si(), o.y->rat->get_den().get_si());
					}

					RPolynomial mult = gf(o.x, len).log(len);
					mult.mul(gf(o.y, len), len);
					return mult.exp(len);
				}
				case Op::Ty::Add:
				case Op::Ty::Sub: {
					RPolynomial l = gf(o.x, len);
					auto ygf = gf(o.y, len);

					if (o.ty==Op::Ty::Sub) l-=ygf; else l+=ygf;
					return l;
				}
				case Op::Ty::Mul: {
					RPolynomial out = gf(o.x, len-o.y->offset);
					out.mul(gf(o.y, len-out.offset), len);
					return out;
				}
				case Op::Ty::Substitute: {
					RPolynomial sub_into = gf(o.x, len/std::max(o.y->offset,1));
					return gf(o.y, len/std::max(sub_into.offset,1)).substitute(sub_into, len);
				}
			}
		},
		[&](Function const& f) {
			int off = f.arg->offset;
			switch (f.ty) {
				case Function::Ty::Exp: return gf(f.arg, std::max(0,len-off)).exp(len);
				case Function::Ty::Cos: return gf(f.arg, std::max(0,len-2*off)).sin_cos(len, false);
				case Function::Ty::Sin: return gf(f.arg, len).sin_cos(len, true);
				case Function::Ty::Log: return gf(f.arg, len).log(len);
				case Function::Ty::Sqrt: return gf(f.arg, len).pow(len,1,2);
				case Function::Ty::Cbrt: return gf(f.arg, len).pow(len,1,3);
				case Function::Ty::Differentiate: {
					RPolynomial p = gf(f.arg, len+1);
					p.derivative();
					return p;
				}
				case Function::Ty::Integrate: {
					RPolynomial p = gf(f.arg, len-1);
					p.integral();
					return p;
				}
				case Function::Ty::Negate: {
					auto g = gf(f.arg, len);
					g.negate();
					return g;
				}
			}
		},
		[&](Var const& x) {
			if (!x.ent.expired()) {
				std::lock_guard lock(*nb_lock);
				if (!x.ent.expired()) {
					auto ptr = x.ent.lock();
					auto out_cast = dynamic_cast<EntryOutput<R> const*>(ptr->output.get());

					if (out_cast) return out_cast->output;
					else return RPolynomial<R>::from(ptr->output->get_poly());
				} else {
					throw MissingDep(x.name);
				}
			}

			if (x.name=="i") {
				return RPolynomial<R>({ctor({0,1})});
			} else return RPolynomial<R>({ctor(1)},1);
		}
	}, ex->v);
}

void Expr::is_rat() {
	rat = std::visit(overloaded {
		[&](mpq_class const& q) {return std::make_optional<mpq_class>(q);},
		[&](Op const& o) {
			o.x->is_rat();
			o.y->is_rat();

			if (!o.x->rat || !o.y->rat) return std::optional<mpq_class>();

			auto& rx =*o.x->rat, &ry=*o.y->rat;

			switch (o.ty) {
				case Op::Ty::Div: return std::make_optional<mpq_class>(rx / ry); break;
				case Op::Ty::Pow:
					if (!ratint(ry))
						return std::optional<mpq_class>();

					return rat_pow(ry.get_num().get_si(),ry.get_den().get_si(),rx); break;
				case Op::Ty::Sub: return std::make_optional<mpq_class>(rx - ry); break;
				case Op::Ty::Add: return std::make_optional<mpq_class>(rx + ry); break;
				case Op::Ty::Mul: return std::make_optional<mpq_class>(rx * ry); break;
				//easy optimization, but im way too lazy for this rn
				case Op::Ty::Substitute: return std::optional<mpq_class>();
			}
		},
		[&](Var const& var) {
			if (var.name!="x" && var.name!="i" && !var.ent.expired()) {
				auto cons = var.ent.lock()->output->is_constant();
				if (cons && !cons->i) return std::optional<mpq_class>(cons->r);
			}

			return std::optional<mpq_class>();
		},
		[&](Function const& f) {
			f.arg->is_rat();
			if (f.arg->rat) {
				mpq_class& rarg = *f.arg->rat;
				switch (f.ty) {
					case Function::Ty::Negate:
						return std::make_optional<mpq_class>(-rarg);
					case Function::Ty::Sqrt:
						return rat_pow(1,2,rarg);
					case Function::Ty::Cbrt:
						return rat_pow(1,3,rarg);
					default: break;
				}
			}

			return std::optional<mpq_class>();
		}
	}, v);
}

Map<std::string, std::monostate> Expr::deps() const {
	return std::visit(overloaded {
			[&](Op const& o) {
				auto oy = o.y->deps();
				auto ox = o.x->deps();
				if (oy.count<ox.count) std::swap(oy,ox);

				for (auto& [k,m]: ox) oy.upsert(k);
				return oy;
			},
			[&](Var const& var) {
				Map<std::string, std::monostate> m;
				if (var.name!="x" && var.name!="i") m.upsert(var.name);
				return m;
			},
			[&](Function const& f) {return f.arg->deps();},
			[&](auto const& x){return Map<std::string, std::monostate>();}
	}, v);
}

void Expr::link(Notebook& nb) {
	std::visit(overloaded {
			[&](Op& o) {
				o.x->link(nb);
				o.y->link(nb);
			},
			[&](Var& var) {
				if (var.name=="x" || var.name=="i") return;

				EntryPtr* vp = nb.names[var.name];
				if (!vp) throw MissingDep(var.name);
				var.ent=*vp;
			},
			[&](Function& f) {f.arg->link(nb);},
			[&](auto& x){}
	}, v);
}

void Notebook::check(std::unique_lock<std::mutex>& lock) {
	for (auto ent: entries) {
		ent->cycle_mark=0;
		if (ent->cycle) {
			ent->mark=-1; //bookkeeping!
			ent->changed=true;
		}
	}

	for (int i=0; i<2*entries.size(); i++) {
		for (auto ent: entries) {
			for (auto ptr: ent->dependents) {
				ptr->cycle_mark=std::max(ptr->cycle_mark, ent->cycle_mark+1);
			}
		}
	}

	Map<EntryPtr, std::monostate> visited;
	Map<EntryPtr, std::monostate> extras;
	std::vector<EntryPtr> to_sort;

	for (EntryPtr k: entries) {
		if (k->cycle_mark<=entries.size() && k->cycle && !k->missing_dep && !k->parse_error) {
			to_sort.push_back(k);
			visited.upsert(k);
		}

		k->cycle=k->cycle_mark>entries.size();
	}

	while (to_sort.size()) {
		auto b = to_sort.back(); to_sort.pop_back();

		for (auto dep: b->dependents) {
			if (dep->cycle || dep->missing_dep || dep->parse_error || visited[dep]) continue;
			visited.upsert(dep);
			to_sort.push_back(dep);
		}
	}
	
	for (auto [k,v]: visited) {
		k->mark=0;
		for (auto& [dk,dv]: k->deps) {
			auto ptr = *names[dk];
			if (visited[ptr]==nullptr && !ptr->job && (ptr->mark==-1 || ptr->mark==ptr->deps.count))
				k->mark++;
		}

		if (k->deps.count==k->mark) Entry::evaluate(k, lock);
	}
}

EntryPtr Notebook::add_entry(std::string name, std::string value, int len, Entry::OutputKind outkind, int float_prec) {
	std::unique_lock lock(*nb_lock);

	EntryPtr ent = std::make_shared<Entry>(Entry {
		.name=name, .nb_lock=nb_lock, .len=len, .cycle=true,
		.out_kind=outkind, .float_prec=float_prec
	});

	ent->parse(value);

	if (ent->ex) {
		ent->deps=ent->ex->deps();
		ent->link(*this);
	}

	if (name.size()) {
		names.insert(ent->name, ent);

		for (EntryPtr ent2: entries) {
			if (ent2->deps[name]) {
				ent->dependents.push_back(ent2);

				if (ent2->missing_dep) ent2->link(*this);
			}
		}
	}

	for (auto& [dep, v]: ent->deps) {
		for (auto fit=names.find_begin(dep); fit!=names.find_end(); ++fit) {
			std::cout<<"dependent "<<fit->name<<" "<<ent->name<<std::endl;
			fit->dependents.push_back(ent);
		}
	}

	entries.push_back(ent);

	check(lock);

	return ent;
}

void Notebook::remove_entry(EntryPtr ent) {
	std::unique_lock lock(*nb_lock);

	ent->cancel();

	for (auto& [k,v]: ent->deps) {
		for (auto fit=names.find_begin(k); fit!=names.find_end(); ++fit) {
			auto& vec = fit->dependents;
			vec.erase(std::find(vec.begin(), vec.end(), ent));
		}
	}

	for (auto it=names.find_begin(ent->name); it!=names.find_end(); ++it) {
		if (*it==ent) {
			it.remove();
			std::cout<<"removed "<<ent->name<<std::endl;
		}
	}

	entries.erase(std::find(entries.begin(), entries.end(),ent));

	for (auto dep: ent->dependents) {
		std::cout<<"relinking dependent "<<dep->name<<std::endl;
		dep->link(*this);
		dep->cycle=true;
	}

	check(lock);
}

void Notebook::update_entry(EntryPtr ent, std::string value, int len, Entry::OutputKind outkind, int float_prec) {
	std::unique_lock lock(*nb_lock);

	ent->parse(value);

	ent->len=len, ent->out_kind=outkind, ent->float_prec=float_prec;

	for (auto& [k,v]: ent->deps) {
		for (auto fit=names.find_begin(k); fit!=names.find_end(); ++fit) {
			fit->dependents.erase(std::find(fit->dependents.begin(), fit->dependents.end(),ent));
		}
	}

	if (ent->ex) {
		ent->deps=ent->ex->deps();
		ent->link(*this);
	} else {
		ent->deps.clear();
	}

	for (auto& [dep, v]: ent->deps) {
		std::cout<<"dependency "<<dep<<std::endl;
		for (auto fit=names.find_begin(dep); fit!=names.find_end(); ++fit) {
			fit->dependents.push_back(ent);
		}
	}

	ent->cycle=true;

	check(lock);
}

void Notebook::rename_entry(EntryPtr ent, std::string new_name) {
	std::unique_lock lock(*nb_lock);

	if (ent->name.size()) {
		for (auto it=names.find_begin(ent->name); it!=names.find_end(); ++it) {
			if (*it==ent) it.remove();
		}

		for (auto dep: ent->dependents) {
			dep->link(*this);
			dep->cycle=true;
		}

		ent->dependents.clear();
	}

	ent->name=new_name;

	if (new_name.size()) {
		names.insert(ent->name, ent);

		for (EntryPtr ent2: entries) {
			if (ent2->deps[new_name]) {
				ent->dependents.push_back(ent2);

				if (ent2->missing_dep) ent2->link(*this), ent2->cycle=true;
			}
		}
	}

	check(lock);
}

//Notebook::~Notebook() {
//	entries.~decltype(entries)();
//	names.~decltype(names)();
//}

void ThreadPool::run() {
	std::unique_lock lock(m, std::defer_lock);
	while (true) {
		lock.lock();

		busy--;

		while (unstarted.empty()) {
			cond_queue.wait(lock);
		}

		busy++;

		JobPtr ptr = unstarted.front();
		unstarted.pop();

		lock.unlock();
		ptr->run(ptr);
		lock.lock();
		ptr->done=true;
		lock.unlock();

		cond_complete.notify_all();
	}
}

#ifdef EMSCRIPTEN

EMSCRIPTEN_BINDINGS(cas) {
	class_<Entry>("Entry")
    .function("status", &Entry::status)
		.function("texInput", &Entry::tex_input)
		.function("getError", &Entry::get_error)
		.function("getOutput", &Entry::get_output)
    .smart_ptr<EntryPtr>("Entry");

	class_<Notebook>("Notebook")
    .constructor()
		.function("addEntry", &Notebook::add_entry)
		.function("removeEntry", &Notebook::remove_entry)
		.function("updateEntry", &Notebook::update_entry)
		.function("renameEntry", &Notebook::rename_entry);

	enum_<Entry::Status>("Status")
    .value("Updated", Entry::Status::Updated)
		.value("NoChange", Entry::Status::NoChange)
    .value("Loading", Entry::Status::Loading);

	enum_<Entry::OutputKind>("OutputKind")
    .value("OGF", Entry::OutputKind::OGF)
		.value("EGF", Entry::OutputKind::EGF)
    .value("Float", Entry::OutputKind::Float);
}

#else

using namespace std::chrono;

int main() {
	time_point tp = high_resolution_clock::now();

	cont = std::make_shared<bool>(true);

	Notebook n;
	n.add_entry("a", "(e^x)", 10, Entry::OutputKind::OGF, 53)->wait();
	n.add_entry("b", "a(1+x)", 10, Entry::OutputKind::Float, 53)->wait();
//	n.add_entry("b", "exp(exp(x)-1)", 300)->wait();
//	n.add_entry("b", "1/(1-x-i)", 1000);

	unsigned ms = duration_cast<milliseconds>(high_resolution_clock::now()-tp).count();
	std::cout<<n.entries[0]->get_output()<<std::endl;
	std::cout<<ms<<" ms"<<std::endl;
}

#endif