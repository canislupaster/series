#define _GLIBCXX_DTS2_CONDITION_VARIABLE_ANY
#include <iostream>
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
	mpq_class cpow;
	int r1 = mpz_root(cpow.get_den_mpz_t(), c.get_den_mpz_t(), std::abs(b));
	int r2 = mpz_root(cpow.get_num_mpz_t(), c.get_num_mpz_t(), std::abs(b));
	if (r1==0 || r2==0)
		return std::optional<mpq_class>();

	mpz_pow_ui(cpow.get_den_mpz_t(), cpow.get_den_mpz_t(), std::abs(a));
	mpz_pow_ui(cpow.get_num_mpz_t(), cpow.get_num_mpz_t(), std::abs(a));

	return cpow;
}

struct IRat {
	mpq_class r, i;

	IRat(mpq_class r=0, mpq_class i=0) : r(r), i(i) {}
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

		mpq_class denom = other.r * other.r + other.i * other.i;
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

template<class Rat=IRat>
struct RPolynomial;

template<class T>
std::ostream& operator<<(std::ostream& os, RPolynomial<T> const& x);

template<class T>
std::optional<mpq_class> to_rat(T const& t) {
	static_assert(False<T>::value, "unimplemented");
}

template<>
std::optional<mpq_class> to_rat<IRat>(IRat const& t) {
	if (t.i) return std::optional<mpq_class>();
	else return std::make_optional(t.r);
}

template<>
std::optional<mpq_class> to_rat<mpq_class>(mpq_class const& t) {
	return std::make_optional(t);
}

struct MulJob;
using MulJobPtr = std::shared_ptr<MulJob>;
struct EvalJob;
using EvalJobPtr = std::shared_ptr<EvalJob>;

struct ThreadPool {
	std::mutex m;
	int busy, n;
	std::condition_variable cond_queue;
	std::condition_variable cond_complete;

	using P = RPolynomial<>*;

	std::queue<MulJobPtr> unstarted_mul;
	std::queue<EvalJobPtr> unstarted_eval;

	void run();

	ThreadPool(int n): n(n), busy(n) {
		for (int i=0; i<n; i++) {
			std::thread thd(&ThreadPool::run, this);
			thd.detach();
		}
	}
};

ThreadPool pool(std::min(std::thread::hardware_concurrency(), 30u));

template<class Rat>
struct RPolynomial {
	std::vector<Rat> coeffs;

	int offset;

	using Vec = std::vector<Rat>;

	thread_local static std::map<int, Vec> free;
	
	void resize(int size) {
		//just in case i add a mpq freelist lmao, probably wont make things any faster tho
		coeffs.resize(size, 0);
	}

	RPolynomial(std::initializer_list<Rat> coeffs, int offset=0): coeffs(coeffs), offset(offset) {}

	RPolynomial(int num_coeffs=0, int offset=0): offset(offset) {
		if (free.empty()) {
			resize(num_coeffs);
			return;
		}
		
		auto it = free.lower_bound(num_coeffs);
		if (it==free.end()) it--;
		this->coeffs.swap(it->second);
		
		free.erase(it);

		resize(num_coeffs);
	}

	RPolynomial(RPolynomial const& other): RPolynomial(other.coeffs.size(), other.offset) {
		coeffs.assign(other.coeffs.begin(), other.coeffs.end());
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
			x=0;

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

		RPolynomial out(len+1,-offset);
		RPolynomial res(len+1);

		len += offset;
		for (int i=0; i<=len && *cont; i++) {
			if (i==0) out.coeffs[i] = Rat(1);
			else out.coeffs[i] = -res.coeffs[i];
			out.coeffs[i] /= coeffs[0];

			for (int j=i+1; j<std::min(len, (int)coeffs.size()+i); j++) {
				res.coeffs[j] += out.coeffs[i]*coeffs[j-i];
			}
		}

		out.canonicalize();
		return out;
	}

	std::optional<Rat> is_constant() {
		if (coeffs.size()>1 || offset!=0) return std::optional<Rat>();
		return std::make_optional(coeffs[0]);
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

		Rat constant = coeffs[-offset];
		x.coeffs[-offset]=0;
		x.canonicalize();

		RPolynomial out = substitute(other, len);
		
		if (x.offset==1) x.coeffs.insert(x.coeffs.begin(), constant);
		else if (-x.offset==x.coeffs.size()) x.coeffs.push_back(constant);
		else x.coeffs[-offset]=constant;

		return out;
	}

	RPolynomial positive_pow(int pow, int len) const {
		if (pow==0) {
			return RPolynomial({Rat(1)}, 0);
		} else if (pow==1) {
			return *this;
		}

		assert(pow>0);

		RPolynomial out({Rat(1)}, 0);
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

		series.coeffs[0] = cpow;
		mpq_class mul = cpow;

		mpq_class k = mpq_class(mpz_class(a), mpz_class(b));
		for (int i=1; i<=len && *cont; i++) {
			mul *= k/(i*c_rat);
			series.coeffs[i] = Rat(mul);
			
			k-=1;
			if (k==0) break;
		}

		return series;
	}

	static RPolynomial exp_series(int len, Rat exp) {
		RPolynomial out(len+1);

		Rat mul=1;
		for (int i=0; i<=len && *cont; i++) {
			out.coeffs[i]=mul;
			mul*=exp/Rat(i+1);
		}

		return out;
	}

	static RPolynomial ln_series(int len, Rat x) {
		RPolynomial series(len+1, 1);

		Rat mul=1;
		for (int i=0; i<=len && *cont; i++) {
			mul*=x;
			if (i%2==1) series.coeffs[i]=-mul/Rat(i+1);
			else series.coeffs[i]=mul/Rat(i+1);
		}

		return series;
	}

	static RPolynomial sin_cos_series(int len, bool sin, Rat x) {
		RPolynomial out(len+1, sin ? 1 : 0);

		Rat mul = x;

		x*=x;
		for (int i=sin ? 1 : 0, term=0; term<=len && *cont; i+=2, term+=2) {
			out.coeffs[term] = mul;
			mul *= -x/Rat((i+1)*(i+2));
		}

		return out;
	}

	bool operator==(RPolynomial const& other) const {
		return coeffs==other.coeffs && offset==other.offset;
	}

	RPolynomial pow(int len, int a, int b) const {
		if (a==0) return RPolynomial({Rat(1)});
		if (a>0 && b>0 && (b==1 || a%b==0)) return positive_pow(a/b, len);

		if (coeffs.empty()) {
			if (a*b>0) return RPolynomial();
			throw ZeroPolynomial();
		}

		if (offset!=0) throw NoConstant();
		RPolynomial series = binom_series(len, a,b, coeffs[0]);

		return substitute_skip(series, len);
	}

	RPolynomial log(int len, Rat mul=1) const {
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

	RPolynomial exp(int len, Rat mul=1) const {
		if (offset<=0) throw ExpNotZero();

		RPolynomial series = exp_series(len, mul);
		return substitute(series, len);
	}

	RPolynomial sin_cos(size_t len, bool sin, Rat mul=1) const {
		if (offset<=0) throw ExpNotZero();

		RPolynomial series = sin_cos_series(len , sin, mul);
		return substitute(series, len);
	}

	template<class T>
	T evaluate(T v) {
		T out=0;
		T xpow=1;
		for (int i=0; i<coeffs.size(); i++) {
			out+=xpow*coeffs[i];
			xpow*=v;
		}

		return out;
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

	~RPolynomial() {
		if (coeffs.size()>100) free.emplace(coeffs.size(), std::move(coeffs));
	}
};

template<>
thread_local std::map<int, std::vector<IRat>> RPolynomial<>::free = {};

struct MulJob {
	RPolynomial<> x;
	RPolynomial<>* incs;
	int len;

	int i, step, num;

	std::shared_ptr<bool> cont_ptr;
	std::weak_ptr<std::vector<MulJobPtr>> completed;
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
	RPolynomial incs[logn+1];
	auto completed = std::make_shared<std::vector<MulJobPtr>>();

	incs[0] = inc;
	for (int i=0; i<=logn && *cont; i++) {
		if (i>0) {
			incs[i] = incs[i-1];
			incs[i].karatsuba_mul(incs[i-1], len);
		}

		{
			std::lock_guard lock(pool.m);
			pool.unstarted_mul.push(std::make_shared<MulJob>(MulJob {.x=base, .incs=incs, .len=len, .i=(1<<i), .step=i, .num=num, .cont_ptr=cont, .completed=completed}));
		}

		pool.cond_queue.notify_one();
	}

	std::unique_lock lock(pool.m);
	for (int i=1; i<num && *cont; ) {
		while (completed->empty() && *cont) {
			pool.cond_complete.wait(lock);
		}

		while (completed->size() && *cont) {
			MulJobPtr j = completed->back();
			completed->pop_back();

			lock.unlock();
			done(j->x, j->i);
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

struct Expr;

enum Precedence: int {
	none=-1,
	addsub=1,
	muldiv=2,
	substitute=3,
	minus=4,
	pow=5,
	nobind=6
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

using ExprPtr = std::shared_ptr<Expr>;
using ConstExprPtr = std::shared_ptr<const Expr>;

struct Function {
	enum Ty {
		Exp,
		Log,
		Sin,
		Cos,
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

struct Entry;
using EntryPtr = std::shared_ptr<Entry>;

struct Var {
	std::string name;
	EntryPtr ent;
};

struct Expr {
	using V = std::variant<mpq_class, Op, Function, Var>;

	V v;
	Precedence prec;
	
	template<class T>
	Expr(T v, Precedence prec=Precedence::nobind): v(v), prec(prec) {}

	explicit Expr() {}

	RPolynomial<> gf(int len) const;
	
	int offset;
	void set_offset();

	std::optional<mpq_class> is_rat() const;

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
//valid. but i may multithread :tm: regular ass operations
struct EvalJob {
	ConstExprPtr to_eval;
	int len;
	std::shared_ptr<bool> cont_ptr;

	RPolynomial<> out;
	std::optional<std::string> err;
	bool done=false;

	std::weak_ptr<Entry> ent;
	std::shared_ptr<std::mutex> nb_lock;

	void run(bool locked);
};

struct Entry {
	std::string name;
	std::shared_ptr<std::mutex> nb_lock;

	ConstExprPtr ex;
	RPolynomial<> output;
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

	EvalJobPtr job;
	bool changed=false;
	std::shared_ptr<bool> eval_cont;

	static void evaluate(EntryPtr ent) {
		std::cout<<"evaluating "<<ent->name<<std::endl;

		ent->cancel();
		ent->eval_cont = std::make_shared<bool>(true);

		ent->eval_error.reset();

		std::cout<<"spawning job"<<std::endl;
		ent->job = std::make_shared<EvalJob>(std::move(EvalJob {
				.to_eval=ent->ex,
				.len=ent->len, .cont_ptr=ent->eval_cont,
				.ent=std::weak_ptr<Entry>(ent), .nb_lock=ent->nb_lock
		}));

		bool b;

		{
			std::lock_guard lock(pool.m);

			b = pool.busy==pool.n;
			std::cout<<pool.busy<<" "<<pool.n<<std::endl;
			if (!b) pool.unstarted_eval.push(ent->job);
		}

		if (b) {
			ent->job->run(true);
		} else {
			pool.cond_queue.notify_one();
		}
	}

	void propagate() {
		for (auto dep: dependents) {
			std::cout<<name<<" (done evaluating) dependend on by "<<dep->name<<" "<<dep->mark<<" "<<dep->deps.count<<std::endl;
			if (!dep->cycle && !dep->missing_dep && (++dep->mark)==dep->deps.count) {
				evaluate(dep);
			}
		}
	}

	void cancel() {
		if (eval_cont) *eval_cont = false;
		if (job) job->ent.reset();
		job.reset();
	}

	void wait() {
		EvalJobPtr p;
		{
			std::lock_guard lock(*nb_lock);
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
		std::ostringstream oss;

		oss<<output.offset<<"\n";

		for (int i=0; i<=std::min((int)output.coeffs.size()-1, len); i++) {
			oss<<output.coeffs[i].r.get_num()<<"\n"<<output.coeffs[i].r.get_den()<<"\n";
			oss<<output.coeffs[i].i.get_num()<<"\n"<<output.coeffs[i].i.get_den()<<"\n";
		}

		return oss.str();
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

			new_ex->set_offset();
			ex=new_ex;

			parse_error.reset();
		} catch (std::exception const& e) {
			ex.reset();
			parse_error=e.what();
		}
	}
};

void EvalJob::run(bool locked) {
	try {
		out = to_eval->gf(len);
	} catch (std::exception const& ex) {
		err.emplace(ex.what());
	}

	if (nb_lock && *cont) {
		std::optional<std::lock_guard<std::mutex>> lock;
		if (!locked) lock.emplace(*nb_lock);
		if (!ent.expired()) {
			auto p = ent.lock();

			if (err) p->eval_error.emplace(*err);
			else p->output = std::move(out);

			std::cout<<"resetting job for "<<p->name<<std::endl;
			p->job.reset();
			p->eval_cont.reset();
			p->changed=true;

			p->propagate();
		}
	}

	{
		std::lock_guard lock(pool.m);
		done=true;
	}
}

struct Notebook {
	std::vector<EntryPtr> entries;
	Map<std::string_view, EntryPtr, true> names;

	std::shared_ptr<std::mutex> nb_lock;

	Notebook(): nb_lock(std::make_shared<std::mutex>()) {}

	EntryPtr add_entry(std::string name, std::string value, int len);
	void remove_entry(EntryPtr ent);
	void update_entry(EntryPtr ent, std::string value, int len);
	void rename_entry(EntryPtr ent, std::string new_name);

	void check();

//	~Notebook();
};

void Expr::tex(std::ostream& os, Precedence min_prec) const {
	if (prec<min_prec) os<<"\\left(";

	std::visit(overloaded {
		[&](mpq_class const& q) {os<<q;},
		[&](Op const& o) {
			if (o.ty==Op::Ty::Div) {
				os<<"\\frac{"; o.x->tex(os, prec); os<<"}{"; o.y->tex(os, prec); os<<"}";
				return;
			}

			os<<"{"; o.x->tex(os, prec); os<<"}";

			switch (o.ty) {
				case Op::Ty::Pow: os<<"^"; break;
				case Op::Ty::Sub: os<<" - "; break;
				case Op::Ty::Add: os<<" + "; break;
				case Op::Ty::Mul: os<<" \\cdot "; break;
				default:;
			}

			os<<"{"; o.y->tex(os, prec); os<<"}";
		},
		[&](Function const& f) {
			switch (f.ty) {
				case Function::Ty::Exp: os<<"e^{"; break;
				case Function::Ty::Cos: os<<"\\cos {"; break;
				case Function::Ty::Sin: os<<"\\sin {"; break;
				case Function::Ty::Log: os<<"\\ln {"; break;
				case Function::Ty::Differentiate: {
					os<<"\\frac{d"; f.arg->tex(os, prec);
					os<<"}{dx}";
					return;
				}
				case Function::Ty::Integrate: {
					os<<"\\(\\int_{0}^{x} ";
					f.arg->tex(os, prec);
					os<<"\\,\\)";
					return;
				}
				case Function::Ty::Negate: os<<"-"; break;
			}

			f.arg->tex(os, prec);
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
						std::optional<mpq_class> rat = o.y->is_rat();
						if (rat && ratint(*rat)) {
							if (rat->get_den()==1) {
								int mul = static_cast<int>(rat->get_num().get_si());
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
					case Function::Ty::Integrate:
						return f.arg->offset+1;
					case Function::Ty::Differentiate:
						return f.arg->offset-1;
					default: return 0;
				}
			},
			[&](Var& x) {
				if (x.ent) return x.ent->output.offset;
				if (x.name=="i") return 0;
				else return 1;
			}
	}, v);
}

RPolynomial<> Expr::gf(int len) const {
	if (!*cont) return RPolynomial();

	auto wrap = [&]() {
		return std::visit(overloaded {
			[&](mpq_class const& q) {
				return len>=0 ? RPolynomial<>({q}) : RPolynomial<>();
			},
			[&](Op const& o) {
				switch (o.ty) {
					case Op::Ty::Div: {
						RPolynomial num = o.x->gf(len+o.y->offset);
						RPolynomial den = o.y->gf(len-num.offset+2*o.y->offset);

						if (den.offset!=o.y->offset) {
							std::cout<<"recomputing values for ur screwed up division operation. sorry! kinda bad at this"<<std::endl;
							num = o.x->gf(len+den.offset);
							den = o.y->gf(len-num.offset+2*den.offset);
						}

						num.mul(den.inv(len-num.offset), len);
						return num;
					}
					case Op::Ty::Pow: {
						std::optional<mpq_class> rat = o.y->is_rat();
						if (rat && ratint(*rat)) {
							return o.x->gf(len).pow(len, rat->get_num().get_si(), rat->get_den().get_si());
						}

						RPolynomial mult = o.x->gf(len).log(len);
						mult.mul(o.y->gf(len), len);
						return mult.exp(len);
					}
					case Op::Ty::Add:
					case Op::Ty::Sub: {
						RPolynomial l = o.x->gf(len);
						auto ygf = o.y->gf(len);

						if (o.ty==Op::Ty::Sub) l-=ygf; else l+=ygf;
						return l;
					}
					case Op::Ty::Mul: {
						RPolynomial out = o.x->gf(len-o.y->offset);
						out.mul(o.y->gf(len-out.offset), len);
						return out;
					}
					case Op::Ty::Substitute: {
						RPolynomial sub_into = o.x->gf(len/o.y->offset);
						return o.y->gf(len/sub_into.offset).substitute(sub_into, len);
					}
				}
			},
			[&](Function const& f) {
				int off = f.arg->offset;
				switch (f.ty) {
					case Function::Ty::Exp: return f.arg->gf(std::max(0,len-off)).exp(len);
					case Function::Ty::Cos: return f.arg->gf(std::max(0,len-2*off)).sin_cos(len, false);
					case Function::Ty::Sin: return f.arg->gf(len).sin_cos(len, true);
					case Function::Ty::Log: return f.arg->gf(len).log(len);
					case Function::Ty::Differentiate: {
						RPolynomial p = f.arg->gf(len+1);
						p.derivative();
						return p;
					}
					case Function::Ty::Integrate: {
						RPolynomial p = f.arg->gf(len-1);
						p.integral();
						return p;
					}
					case Function::Ty::Negate: {
						auto g = f.arg->gf(len);
						g.negate();
						return g;
					}
				}
			},
			[&](Var const& x) {
				if (x.ent) return x.ent->output;
				if (x.name=="i") return RPolynomial<>({IRat(0,1)});
				return RPolynomial<>({IRat(1)},1);
			}
		}, v);
	}();

	return wrap;
}

std::optional<mpq_class> Expr::is_rat() const {
	return std::visit(overloaded {
		[&](mpq_class const& q) {return std::make_optional<mpq_class>(q);},
		[&](Op const& o) {
			std::optional<mpq_class> rx = o.x->is_rat();
			if (!rx) return std::optional<mpq_class>();
			std::optional<mpq_class> ry = o.y->is_rat();
			if (!ry) return std::optional<mpq_class>();

			switch (o.ty) {
				case Op::Ty::Div: return std::make_optional<mpq_class>(*rx / *ry); break;
				case Op::Ty::Pow:
					if (!ratint(*ry))
						return std::optional<mpq_class>();

					return rat_pow(ry->get_num().get_si(),ry->get_den().get_si(),*rx); break;
				case Op::Ty::Sub: return std::make_optional<mpq_class>(*rx - *ry); break;
				case Op::Ty::Add: return std::make_optional<mpq_class>(*rx + *ry); break;
				case Op::Ty::Mul: return std::make_optional<mpq_class>(*rx * *ry); break;
				//easy optimization, but im way too lazy for this rn
				case Op::Ty::Substitute: return std::optional<mpq_class>();
			}
		},
		[&](Var const& var) {
			if (var.name!="x" && var.name!="i" && var.ent) {
				auto cons = var.ent->output.is_constant();
				if (cons && !cons->i) return std::optional<mpq_class>(cons->r);
			}

			return std::optional<mpq_class>();
		},
		[&](Function const& f) {
			if (f.ty==Function::Ty::Negate) {
				std::optional<mpq_class> rarg = f.arg->is_rat();
				if (rarg) return std::make_optional<mpq_class>(-*rarg);
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

void Notebook::check() {
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

		if (k->deps.count==k->mark) Entry::evaluate(k);
	}
}

EntryPtr Notebook::add_entry(std::string name, std::string value, int len) {
	std::lock_guard lock(*nb_lock);

	EntryPtr ent = std::make_shared<Entry>(Entry {.name=name, .nb_lock=nb_lock, .len=len, .cycle=true});

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

	check();

	return ent;
}

void Notebook::remove_entry(EntryPtr ent) {
	std::lock_guard lock(*nb_lock);

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

	check();
}

void Notebook::update_entry(EntryPtr ent, std::string value, int len) {
	std::lock_guard lock(*nb_lock);

	ent->parse(value);

	ent->len=len;

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

	check();
}

void Notebook::rename_entry(EntryPtr ent, std::string new_name) {
	std::lock_guard lock(*nb_lock);

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
		names.insert(new_name, ent);

		for (EntryPtr ent2: entries) {
			if (ent2->deps[new_name]) {
				ent->dependents.push_back(ent2);

				if (ent2->missing_dep) ent2->link(*this), ent2->cycle=true;
			}
		}
	}

	check();
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

		while (unstarted_mul.empty() && unstarted_eval.empty()) {
			cond_queue.wait(lock);
		}

		busy++;

		if (unstarted_mul.size()) {
			MulJobPtr ptr = unstarted_mul.front();
			unstarted_mul.pop();
			MulJob& j = *ptr;
			cont = j.cont_ptr;

			lock.unlock();

			j.x.karatsuba_mul(j.incs[j.step], j.len);

			lock.lock();

			int num_job=0;
			auto completed = j.completed.lock();
			if (completed && *cont) {
				completed->push_back(ptr);

				for (int i=0; i<j.step; i++) {
					MulJob newjob = j;
					newjob.i=j.i+(1<<i), newjob.step=i;

					if (newjob.i>=j.num) break;

					pool.unstarted_mul.push(std::make_shared<MulJob>(std::move(newjob)));
					num_job++;
				}
			}

			lock.unlock();

			for (int i=1; i<num_job; i++) cond_queue.notify_one();
		} else {
			EvalJobPtr ptr = unstarted_eval.front();
			unstarted_eval.pop();
			lock.unlock();

			cont = ptr->cont_ptr;
			ptr->run(false);
		}

		cond_complete.notify_all();
	}
}

#ifdef EMSCRIPTEN
//Notebook nb;
//
//char* tocstr(std::string const& str) {
//	char* cstr = new char[str.size()+1];
//	std::copy(str.c_str(), str.c_str()+str.size()+1, cstr);
//	return cstr;
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" EntryPtr add_ent(const char* name, const char* value, int len) {
////	std::cout<<mpz_class(5)*mpz_class(10)<<std::endl;
////	std::cout<<(mpz_class(5)<<3)<<std::endl;
////	std::cout<<numbits(mpz_class(7))<<std::endl;
////
////	mpz_class x(15);
////	mpz_tdiv_r_2exp(x.get_mpz_t(), x.get_mpz_t(), 2);
////	std::cout<<x<<std::endl;
//
//	return nb.add_entry(name, value, len);
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" void up_ent(EntryPtr ent, const char* value, int len) {
//	return nb.update_entry(ent, value, len);
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" void del_ent(EntryPtr ent) {
//	return nb.remove_entry(ent);
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" char const* get_state(EntryPtr ent) {
//	if (ent->changed) {
//		ent->changed=false;
//		return "changed";
//	} else {
//		return false;
//	}
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" char const* entry_error(EntryPtr ent) {
//	if (ent->parse_error) return ent->parse_error->c_str();
//	else if (ent->missing_dep) return ent->missing_dep->what();
//	else if (ent->cycle) return "dependency cycle";
//	else if (ent->eval_error) return ent->eval_error->c_str();
//	else return nullptr;
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" char* get_tex_input(EntryPtr ent) {
//	std::ostringstream oss;
//	ent->ex->tex(oss);
//	return tocstr(oss.str());
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" char* get_output(EntryPtr ent) {
//	std::ostringstream oss;
//
//	RPolynomial<>& x = ent->output;
//
//	ent->ex->tex(std::cout);
//	std::cout<<std::endl;
//	std::cout<<x<<std::endl;
//
//	for (int j=0; j<x.offset; j++) {
//		oss<<"\n\n";
//	}
//
//	for (int i=0; i<std::min(x.coeffs.size(), ent->len); i++) {
//		mpz_class gcd;
//		mpz_gcd(gcd.get_mpz_t(), x.coeffs[i].get_mpz_t(), x.den.get_mpz_t());
//		mpz_class num = x.coeffs[i]/gcd; mpz_class den=ent->output.den/gcd;
//		oss<<num<<"\n"<<den<<"\n";
//	}
//
//	return tocstr(oss.str());
//}
//
//EMSCRIPTEN_KEEPALIVE
//extern "C" void clear_nb() {
//	nb = Notebook();
//}

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
}

#else

using namespace std::chrono;

int main() {
	time_point tp = high_resolution_clock::now();

	cont = std::make_shared<bool>(true);

	Notebook n;
	n.add_entry("a", "sin(1+x) + ln(1/x^2) - cos(x)+e^(x^2+1-1)+int(d(sin(x)))", 50)->ex->tex(std::cout);
//	n.add_entry("b", "a+1", 50)->wait();
//	n.add_entry("b", "exp(exp(x)-1)", 300)->wait();
//	n.add_entry("b", "1/(1-x-i)", 1000);

	unsigned ms = duration_cast<milliseconds>(high_resolution_clock::now()-tp).count();
	std::cout<<n.entries[0]->output<<std::endl;
	std::cout<<ms<<" ms"<<std::endl;
}

#endif