#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <string_view>
#include <stack>
#include <tgmath.h>
#include <chrono>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/val.h>
#endif

#include <gmp.h>
#include <gmpxx.h>

#include "util.hpp"
#include "map.hpp"

size_t numbits(mpz_class const& x) {
	return mpz_sizeinbase(x.get_mpz_t(), 2);
}

struct IrrationalPower: public std::exception {
	char const* what() const noexcept {
		return "irrational power. (it might be rational but it would require irrationals to express)";
	}
};

struct NoConstant: public std::exception {
	char const* what() const noexcept {
		return "inverting & taking to nonintegral powers, etc require a nonzero constant bc otherwise (higher derivative) no converge around 0 -> undefined coeffs smh";
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

struct RPolynomial;
std::ostream& operator<<(std::ostream& os, RPolynomial const& x);

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

struct RPolynomial {
	std::vector<mpz_class> coeffs;
	mpz_class den = 1;

	void canonicalize() {
		mpz_class gcd = den;
		for (mpz_class& x: coeffs) mpz_gcd(gcd.get_mpz_t(), x.get_mpz_t(), gcd.get_mpz_t());

		den/=gcd;
		for (mpz_class& x: coeffs) x/=gcd;

		auto end_it = coeffs.end();
		while (end_it!=coeffs.begin() && mpz_sgn(prev(end_it)->get_mpz_t())==0) {
			end_it=coeffs.erase(prev(end_it));
		}
	}

	void operator+=(RPolynomial const& other) {
		coeffs.resize(std::max(coeffs.size(), other.coeffs.size()));
		for (size_t i=0; i<other.coeffs.size(); i++) {
			coeffs[i]=den*other.coeffs[i] + coeffs[i]*other.den;
		}

		den*=other.den;

		canonicalize();
	}

	void operator-=(RPolynomial const& other) {
		coeffs.resize(std::max(coeffs.size(), other.coeffs.size()));
		for (size_t i=0; i<other.coeffs.size(); i++) {
			coeffs[i]=coeffs[i]*other.den - den*other.coeffs[i];
		}

		den*=other.den;

		canonicalize();
	}

	std::pair<mpz_class, mpz_class> concat(size_t stride, bool skip_first=false) const {
		mpz_class outp=0,outn=0;
		for (int i=skip_first ? 1 : 0; i<coeffs.size(); i++) {
			if (mpz_sgn(coeffs[i].get_mpz_t())>=0) outp+=coeffs[i] << (i*stride);
			else outn+=(-coeffs[i]) << (i*stride);
		}

		return std::make_pair(outp, outn);
	}

	using P = std::pair<mpz_class, mpz_class>;

	static RPolynomial deconcat(P x, mpz_class lcm, size_t stride) {
		RPolynomial out {.den=lcm};

		size_t precp = numbits(x.first), precn=numbits(x.second);
		for (int i=0; i<std::max(precp,precn); i+=stride) {
			mpz_class& y = out.coeffs.emplace_back(0);

			if (i<precp) {
				mpz_tdiv_r_2exp(y.get_mpz_t(), x.first.get_mpz_t(), stride);
				x.first>>=stride;
			}

			if (i<precn) {
				mpz_class sub;
				mpz_tdiv_r_2exp(sub.get_mpz_t(), x.second.get_mpz_t(), stride);

				y-=sub;
				x.second>>=stride;
			}
		}

		out.canonicalize();
		return out;
	}

	static RPolynomial from_rats(std::vector<mpq_class>& vec) {
		RPolynomial out = {.coeffs=std::vector<mpz_class>(vec.size()), .den=1};

		for (int i=0; i<vec.size(); i++) mpz_lcm(out.den.get_mpz_t(), out.den.get_mpz_t(), vec[i].get_den_mpz_t());

		for (int i=0; i<vec.size(); i++) {
			out.coeffs[i]=vec[i].get_num()*(out.den/vec[i].get_den());
		}

		out.canonicalize();
		return out;
	}

	size_t max_prec() const {
		size_t prec=0;
		for (mpz_class const& x: coeffs) if (numbits(x)>prec) prec=numbits(x);
		return prec;
	}

	static P pair_mul(P const& a, P const& b) {
		return std::make_pair(a.first*b.first + a.second*b.second, a.first*b.second + a.second*b.first);
	}

	static P pair_muls(P const& a, mpz_class const& b) {
		if (mpz_sgn(b.get_mpz_t())<0) return std::make_pair((-b)*a.second, (-b)*a.first);
		else return std::make_pair(b*a.first, b*a.second);
	}

	static void pair_trunc(P& x, int len, int stride) {
		mpz_mod_2exp(x.first.get_mpz_t(), x.first.get_mpz_t(), len*stride);
		mpz_mod_2exp(x.second.get_mpz_t(), x.second.get_mpz_t(), len*stride);
	}

	RPolynomial operator*(RPolynomial const& other) const {
		size_t stride = max_prec() + other.max_prec() + std::log(std::min(coeffs.size(), other.coeffs.size()));

		std::cout<<*this<<" * "<<other<<" = ";
		auto a = concat(stride), b=other.concat(stride);

		auto res = deconcat(pair_mul(a,b), den*other.den, stride);
		std::cout<<res<<"\n";
		return res;
	}

	RPolynomial substitute(RPolynomial const& other, int len, bool skip_first=false) const {
		if (other.coeffs.size()<=1) return other;
		if (coeffs.empty()) {
			RPolynomial out = {.coeffs={other.coeffs[0]}, .den=other.den};
			out.canonicalize();
			return out;
		};

		size_t stride = other.max_prec() + (other.coeffs.size()-1)*(max_prec()+numbits(den)) + std::log(coeffs.size())*(other.coeffs.size()-1) + 1;

		P a = std::make_pair(1,0);
		P b=concat(stride, skip_first);

		std::vector<mpz_class> lcm_pows(other.coeffs.size()-1);
		lcm_pows[0]=den;
		for (int i=1; i<lcm_pows.size(); i++) lcm_pows[i]=lcm_pows[i-1]*den;

		RPolynomial fst = {.coeffs={other.coeffs[0]*lcm_pows[other.coeffs.size()-2]}};
		P out = fst.concat(stride);

		for (int i=1; i<other.coeffs.size(); i++) {
			a=pair_mul(a,b);
			pair_trunc(a, len, stride);

			P add;
			if (i<other.coeffs.size()-1) {
				add=pair_muls(a,other.coeffs[i]*lcm_pows[other.coeffs.size()-i-2]);
			} else {
				add=pair_muls(a, other.coeffs[i]);
			}

			pair_trunc(add, len, stride);

			out.first+=add.first;
			out.second+=add.second;

			pair_trunc(out, len, stride);
		}

		return deconcat(out, other.den*lcm_pows[other.coeffs.size()-2], stride);
	}

	RPolynomial positive_pow(int len, unsigned pow) const {
		if (pow==0) {
			RPolynomial out; out.coeffs.push_back(1);
			return out;
		} else if (pow==1) {
			return *this;
		}

		size_t stride = pow*max_prec() + (pow-1)*std::log(coeffs.size());

		mpz_class lcm_pow;
		mpz_pow_ui(lcm_pow.get_mpz_t(), den.get_mpz_t(), pow);

		P a = concat(stride);
		P b = std::make_pair(1,0);
		for (; pow>0; pow>>=1) {
			if (pow&1) {
				b=pair_mul(a,b);
				pair_trunc(b, len, stride);
			}

			a=pair_mul(a,a);
			pair_trunc(a, len, stride);
		}

		return deconcat(b, lcm_pow, stride);
	}

	RPolynomial inv(size_t len) const {
		if (mpz_sgn(coeffs[0].get_mpz_t())==0) throw NoConstant();

		std::vector<mpq_class> out;

		std::vector<mpq_class> res(len, 0);

		for (int i=0; i<len; i++) {
			if (i==0) out.push_back(mpq_class(den, coeffs[0]));
			else out.push_back(-res[i]/coeffs[0]);

			for (int j=i+1; j<std::min(len, coeffs.size()+i); j++) {
				res[j] += out.back()*coeffs[j-i];
			}
		}

		return from_rats(out);
	}

	static RPolynomial binom_series(size_t len, int a, int b, mpq_class c) {
		std::vector<mpq_class> series;

		std::optional<mpq_class> cpow_opt = rat_pow(a,b,c);
		if (!cpow_opt) throw IrrationalPower();
		mpq_class cpow=*cpow_opt;

		if (std::signbit(a)!=std::signbit(b)) {
			cpow=1/cpow;
		}

		series.push_back(c);

		mpq_class k = mpq_class(mpz_class(a), mpz_class(b));
		for (int i=1; i<len; i++) {
			series.push_back(series[i-1]*k/(i*cpow));
			k-=1;

			if (k==0) break;
		}

		return from_rats(series);
	}

	static RPolynomial exp_series(size_t len, mpq_class exp) {
		RPolynomial out = {.coeffs=std::vector<mpz_class>(len, 1), .den=1};
		for (int i=len-2; i>=0; i--) {
			out.coeffs[i]=out.coeffs[i+1]*(i+1)*exp.get_den();
		}

		mpz_class mul=exp.get_num();
		for (int i=1; i<len; i++) {
			out.coeffs[i]*=mul;
			mul*=exp.get_num();
		}

		out.den=out.coeffs[0];
		return out;
	}

	static RPolynomial ln_series(size_t len, mpq_class x) {
		std::vector<mpq_class> series(len);
		mpq_class mul=1;
		for (int i=1; i<len; i++) {
			mul*=x;
			if (i%2==0) series[i]=-mul/i;
			else series[i]=mul/i;
		}

		return from_rats(series);
	}

	static RPolynomial sin_cos_series(size_t len, mpq_class x, bool sin) {
		RPolynomial out = {.coeffs=std::vector<mpz_class>(len, 0), .den=1};
		std::vector<mpz_class> dens={sin ? x.get_den() : 1};

		x*=x;
		for (int i=sin ? 3 : 2; i<len; i+=2) {
			dens.push_back(dens.back()*i*(i-1)*x.get_den());
		}

		mpz_class mul=sin ? x.get_num() : 1;
		for (int i=sin ? 1 : 0; i<len; i+=2) {
			out.coeffs[i]=mul*(dens.back()/dens[i/2]);
			mul*=x.get_num();
		}

		out.den=dens.back();
		return out;
	}

	bool operator==(RPolynomial const& other) const {
		return coeffs==other.coeffs && den==other.den;
	}

	RPolynomial pow(size_t len, int a, int b) const {
		if (a>0 && b>0 && (b==1 || a%b==0)) return positive_pow(len, a/b);

		if (mpz_sgn(coeffs[0].get_mpz_t())==0) throw NoConstant();
		RPolynomial series = binom_series(len, a,b, coeffs[0]);
		return substitute(series, len, true);
	}

	RPolynomial log(size_t len, mpq_class mul=1) const {
		if (coeffs[0]!=1) throw LogNotOne();

		RPolynomial series = ln_series(len, mul);
		return substitute(series, len, true);
	}

	RPolynomial exp(size_t len, mpq_class mul=1) const {
		if (coeffs[0]!=0) throw ExpNotZero();

		RPolynomial series = exp_series(len, mul);
		return substitute(series, len, true);
	}

	RPolynomial sin_cos(size_t len, bool sin, mpq_class mul=1) const {
		if (coeffs[0]!=0) throw ExpNotZero();

		RPolynomial series = sin_cos(len, sin, mul);
		return substitute(series, len, true);
	}

	template<class T>
	T evaluate(T v) {
		T out=0;
		T xpow=1;
		for (int i=0; i<coeffs.size(); i++) {
			out+=xpow*coeffs[i];
			xpow*=v;
		}

		return out/den;
	}
};

std::ostream& operator<<(std::ostream& os, RPolynomial const& x) {
	bool added=false;
	for (int i=0; i<x.coeffs.size(); i++) {
		int sgn = mpz_sgn(x.coeffs[i].get_mpz_t());
		if (sgn==0) continue;

		mpq_class rat(x.coeffs[i], x.den);
		rat.canonicalize();

		mpq_abs(rat.get_mpq_t(), rat.get_mpq_t());

		if (sgn>0) {
			os<<(added ? " + " : "")<<rat.get_num()<<"*x^"<<i<<"/"<<rat.get_den();
		} else if (sgn<0) {
			os<<(added ? " - " : "-")<<rat.get_num()<<"*x^"<<i<<"/"<<rat.get_den();
		}

		added=true;
	}

	return os;
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

struct Op; struct Function; struct Var;

enum Precedence: int {
	none=-1,
	addsub=1,
	muldiv=2,
	minus=3,
	pow=4,
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

struct Expr {
	using V = std::variant<mpq_class, Op, Function, Var>;

	std::unique_ptr<V> v;

	template<class T>
	Expr(T v);

	explicit Expr() {}

	void tex(std::ostream& os) const;

	RPolynomial gf(int len) const;

	std::optional<mpq_class> is_rat() const;

	Map<std::string, std::monostate> deps() const;
	void link(Notebook& nb);
};

struct Function {
	enum Ty {
		Exp,
		Log,
		Sin,
		Cos
	};

	Ty ty;
	Expr arg;
};

struct Op {
	enum Ty {
		Add,Sub,Div,Mul,Pow
	};

	Ty ty;

	Expr x;
	Expr y;
};

struct Entry;

struct Var {
	std::string name;
	Entry* ent;
};

template<class T>
Expr::Expr(T v): v(new Expr::V(v)) {}

Expr parse(const char** str, Precedence prec) {
	Expr left;

	skip_ws(str);
	const char* begin=*str;
	if (starts_with(str, "-")) {
		left.v.reset(new Expr::V(std::move(Op {.ty=Op::Ty::Sub, .x=Expr(), .y=parse(str, Precedence::minus)})));
	} else if (starts_with(str, "e^")) {
		left.v.reset(new Expr::V(std::move(Function {.ty=Function::Exp, .arg=parse(str, Precedence::pow)})));
	} else if (starts_with(str, "exp(")) {
		left.v.reset(new Expr::V(std::move(Function {.ty=Function::Exp, .arg=parse(str, Precedence::none)})));
	} else if (starts_with(str, "ln(")) {
		left.v.reset(new Expr::V(std::move(Function {.ty=Function::Log, .arg=parse(str, Precedence::none)})));
	} else if (starts_with(str, "sin(")) {
		left.v.reset(new Expr::V(std::move(Function {.ty=Function::Sin, .arg=parse(str, Precedence::none)})));
	} else if (starts_with(str, "cos(")) {
		left.v.reset(new Expr::V(std::move(Function {.ty=Function::Cos, .arg=parse(str, Precedence::none)})));
	} else if (starts_with(str, "(")) {
		left=std::move(parse(str, Precedence::none));
	} else if (isalpha(**str)) {
		while (isalnum(**str)) (*str)++;
		std::string name(begin, *str);
		left.v.reset(new Expr::V(Var {.name=name}));
	} else {
		while (isdigit(**str) || **str=='e' || **str=='.') (*str)++;

		left=mpq_class();
		std::get<mpq_class>(*left.v).set_str(std::string(begin, *str), 10);
		if (*str==begin) throw ExpectedSomething();
	}

	while (true) {
		skip_ws(str);
		if (prec<Precedence::addsub && starts_with(str, "+")) {
			left.v.reset(new Expr::V(std::move(Op {.ty=Op::Ty::Add, .x=std::move(left), .y=parse(str, Precedence::addsub)})));
		} else if (prec<Precedence::addsub && starts_with(str, "-")) {
			left.v.reset(new Expr::V(std::move(Op {.ty=Op::Ty::Sub, .x=std::move(left), .y=parse(str, Precedence::addsub)})));
		} else if (prec<Precedence::muldiv && starts_with(str, "/")) {
			left.v.reset(new Expr::V(std::move(Op {.ty=Op::Ty::Div, .x=std::move(left), .y=parse(str, Precedence::muldiv)})));
		} else if (prec<Precedence::muldiv && starts_with(str, "*")) {
			left.v.reset(new Expr::V(std::move(Op {.ty=Op::Ty::Mul, .x=std::move(left), .y=parse(str, Precedence::muldiv)})));
		} else if (prec<Precedence::pow && starts_with(str, "^")) {
			left.v.reset(new Expr::V(std::move(Op {.ty=Op::Ty::Pow, .x=std::move(left), .y=parse(str, Precedence::pow)})));
		} else if (prec==Precedence::none) {
			starts_with(str, ")");
			break;
		} else {
			break;
		}
	}

	return left;
}

struct Entry {
	std::string name;

	Expr ex;
	RPolynomial output;
	size_t len;

	int mark;

	std::optional<std::string> parse_error;
	std::optional<std::string> eval_error;

	bool cycle;
	std::optional<MissingDep> missing_dep;
	std::vector<Entry*> dependents;
	Map<std::string, std::monostate> deps;

	bool changed=false;
	void evaluate() {
		try {
			output = ex.gf(len);
			changed=true;
			eval_error.reset();
		} catch (std::exception const& err) {
			eval_error = err.what();
		}
	}

	void link(Notebook& nb) {
		try {
			ex.link(nb);
			missing_dep.reset();
		} catch (MissingDep d) {
			missing_dep=d;
		}
	}

	void parse(std::string const& value) {
		const char* cstr = value.c_str();

		try {
			ex = std::move(::parse(&cstr, Precedence::none));
			if (strlen(cstr)) throw std::runtime_error("unexpected stuff while parsing");

			parse_error.reset();
		} catch (std::exception const& e) {
			ex=Expr();
			parse_error=e.what();
		}
	}
};

struct Notebook {
	std::vector<Entry*> entries;
	Map<std::string_view, Entry*, true> names;

	Notebook() {}

	Entry* add_entry(std::string name, std::string value, size_t len);
	void remove_entry(Entry* ent);
	void update_entry(Entry* ent, std::string value, int len);

	void check();

	~Notebook();
};

void Expr::tex(std::ostream& os) const {
	if (!v) return;

	std::visit(overloaded {
		[&](mpq_class const& q) {os<<q;},
		[&](Op const& o) {
			if (o.ty==Op::Ty::Div) {
				os<<"\\frac{"; o.x.tex(os); os<<"}{"; o.y.tex(os); os<<"}";
				return;
			}

			os<<"{"; o.x.tex(os); os<<"}";

			switch (o.ty) {
				case Op::Ty::Pow: os<<"^"; break;
				case Op::Ty::Sub: os<<" - "; break;
				case Op::Ty::Add: os<<" + "; break;
				case Op::Ty::Mul: os<<" \\cdot "; break;
				default:;
			}

			os<<"{"; o.y.tex(os); os<<"}";
		},
		[&](Function const& f) {
			switch (f.ty) {
				case Function::Ty::Exp: os<<"e^{"; break;
				case Function::Ty::Cos: os<<"cos("; break;
				case Function::Ty::Sin: os<<"sin("; break;
				case Function::Ty::Log: os<<"ln("; break;
			}

			f.arg.tex(os);
			os<<(f.ty==Function::Ty::Exp ? "}" : ")");
		},
		[&](Var const& x) {os<<x.name;}
	}, *v);
}

bool ratint(mpq_class const& rat) {
	return rat.get_num().fits_sint_p() && rat.get_den().fits_sint_p();
}

RPolynomial Expr::gf(int len) const {
	std::cout<<"finding gf for ";
	tex(std::cout); std::cout<<std::endl;

	if (!v) return RPolynomial {.coeffs={}, .den=1};

	return std::visit(overloaded {
		[&](mpq_class const& q) {
			return RPolynomial {.coeffs={q.get_num()}, .den=q.get_den()};
		},
		[&](Op const& o) {
			switch (o.ty) {
				case Op::Ty::Div: return o.x.gf(len)*o.y.gf(len).inv(len); break;
				case Op::Ty::Pow: {
					std::optional<mpq_class> rat = o.y.is_rat();
					if (rat && ratint(*rat)) {
						return o.x.gf(len).pow(len, rat->get_num().get_si(), rat->get_den().get_si());
					}

					return (o.x.gf(len).log(len)*o.y.gf(len)).exp(len);
					break;
				}
				case Op::Ty::Sub: {
					RPolynomial l = o.x.gf(len);
					l-=o.y.gf(len);
					return l;

					break;
				}
				case Op::Ty::Add: {
					RPolynomial l = o.x.gf(len);
					l+=o.y.gf(len);
					return l;

					break;
				}
				case Op::Ty::Mul: return o.x.gf(len)*o.y.gf(len); break;
			}
		},
		[&](Function const& f) {
			switch (f.ty) {
				case Function::Ty::Exp: return f.arg.gf(len).exp(len); break;
				case Function::Ty::Cos: return f.arg.gf(len).sin_cos(len, false); break;
				case Function::Ty::Sin: return f.arg.gf(len).sin_cos(len, true); break;
				case Function::Ty::Log: return f.arg.gf(len).log(len); break;
			}
		},
		[&](Var const& x) {
			if (x.ent) return x.ent->output;
			return RPolynomial {.coeffs={0,1},.den={1}};
		}
	}, *v);
}

std::optional<mpq_class> Expr::is_rat() const {
	if (!v) return std::make_optional<mpq_class>(0);

	return std::visit(overloaded {
		[&](mpq_class const& q) {return std::make_optional<mpq_class>(q);},
		[&](Op const& o) {
			std::optional<mpq_class> rx = o.x.is_rat();
			if (!rx) return std::optional<mpq_class>();
			std::optional<mpq_class> ry = o.y.is_rat();
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
			}
		},
		[&](Var const& var) {
			if (var.name!="x" && var.ent->output.coeffs.size()==1) {
				return std::make_optional<mpq_class>(var.ent->output.coeffs[0]);
			} else {
				return std::optional<mpq_class>();
			}
		},
		[&](Function const& f) {
			return std::optional<mpq_class>();
		}
	}, *v);
}

Map<std::string, std::monostate> Expr::deps() const {
	if (!v) return {};

	return std::visit(overloaded {
			[&](Op const& o) {
				auto oy = o.y.deps();
				auto ox = o.x.deps();
				if (oy.count<ox.count) std::swap(oy,ox);

				for (auto& [k,m]: ox) oy.upsert(k);
				return oy;
			},
			[&](Var const& var) {
				Map<std::string, std::monostate> m;
				if (var.name!="x") m.upsert(var.name);
				return m;
			},
			[&](Function const& f) {return f.arg.deps();},
			[&](auto const& x){return Map<std::string, std::monostate>();}
	}, *v);
}

void Expr::link(Notebook& nb) {
	std::visit(overloaded {
			[&](Op& o) {
				o.x.link(nb);
				o.y.link(nb);
			},
			[&](Var& var) {
				if (var.name=="x") return;

				Entry** vp = nb.names[var.name];
				if (!vp) throw MissingDep(var.name);
				var.ent=*vp;
			},
			[&](Function& f) {f.arg.link(nb);},
			[&](auto& x){}
	}, *v);
}

void Notebook::check() {
	for (auto ent: entries) {
		ent->mark=0;
	}

	for (int i=0; i<2*entries.size(); i++) {
		for (auto ent: entries) {
			for (auto ptr: ent->dependents) {
				ptr->mark=std::max(ptr->mark, ent->mark+1);
			}
		}
	}

	Map<Entry*, std::monostate> visited;
	Map<Entry*, std::monostate> extras;
	std::vector<Entry*> to_sort;

	for (Entry* k: entries) {
		if (k->mark<=entries.size() && k->cycle && !k->missing_dep && !k->parse_error) {
			to_sort.push_back(k);
			visited.upsert(k);
		}

		k->cycle=k->mark>entries.size();
	}

	std::vector<Entry*> topo;
	while (to_sort.size()) {
		auto b = to_sort.back(); to_sort.pop_back();
		b->mark=0;
		if (b->deps.count==0) topo.push_back(b);
		else for (auto& [k,v]: b->deps) {
			extras.upsert(*names[k]);
		}

		for (auto dep: b->dependents) {
			if (dep->cycle || dep->missing_dep || dep->parse_error || visited[dep]) continue;
			visited.upsert(dep);
			to_sort.push_back(dep);
		}
	}

	for (auto [k,v]: visited) {
		extras.remove(k);
	}

	for (auto [k,v]: extras) {
		topo.push_back(k);
	}

	while (topo.size()) {
		auto b = topo.back(); topo.pop_back();
		if (extras[b]==nullptr) b->evaluate();

		for (auto dep: b->dependents) {
			std::cout<<b->name<<" dependend on by "<<dep->name<<" "<<dep->mark<<" "<<dep->deps.count<<std::endl;
			if (!dep->cycle && !dep->missing_dep && (++dep->mark)==dep->deps.count) {
				topo.push_back(dep);
			}
		}
	}
}

Entry* Notebook::add_entry(std::string name, std::string value, size_t len) {
	Entry* ent = new Entry {.name=name, .ex=Expr(), .len=len, .cycle=true};

	ent->parse(value);

	ent->deps=ent->ex.deps();

	if (name.size()) {
		names.insert(ent->name, ent);

		for (Entry* ent2: entries) {
			if (ent2->deps[name]) {
				ent->dependents.push_back(ent2);

				if (ent2->missing_dep) ent2->link(*this);
			}
		}
	}

	for (auto [k,v]: names) {
		std::cout<<"\""<<k<<"\" is a name"<<std::endl;
	}

	for (auto& [dep, v]: ent->deps) {
		for (auto fit=names.find_begin(dep); fit!=names.find_end(); ++fit) {
			std::cout<<"dependent "<<fit->name<<" "<<ent->name<<std::endl;
			fit->dependents.push_back(ent);
		}
	}

	ent->link(*this);

	entries.push_back(ent);

	check();

	return ent;
}

void Notebook::remove_entry(Entry* ent) {
	for (auto& [k,v]: ent->deps) {
		for (auto fit=names.find_begin(k); fit!=names.find_end(); ++fit) {
			auto& vec = fit->dependents;
			vec.erase(std::find(vec.begin(), vec.end(), ent));
		}
	}

	for (auto it=names.begin(); it!=names.end(); ++it) {
		if (it->second==ent) it.remove();
	}

	entries.erase(std::find(entries.begin(), entries.end(),ent));

	for (auto dep: ent->dependents) {
		dep->link(*this);
	}

	check();

	delete ent;
}

void Notebook::update_entry(Entry* ent, std::string value, int len) {
	ent->parse(value);

	ent->len=len;

	for (auto& [k,v]: ent->deps) {
		for (auto fit=names.find_begin(k); fit!=names.find_end(); ++fit) {
			fit->dependents.erase(std::find(fit->dependents.begin(), fit->dependents.end(),ent));
		}
	}

	ent->deps=ent->ex.deps();

	for (auto& [dep, v]: ent->deps) {
		std::cout<<"dependency "<<dep<<std::endl;
		for (auto fit=names.find_begin(dep); fit!=names.find_end(); ++fit) {
			fit->dependents.push_back(ent);
		}
	}

	ent->link(*this);

	ent->cycle=true;

	check();
}

Notebook::~Notebook() {
	for (Entry* ent: entries) delete ent;
}

#ifdef EMSCRIPTEN
Notebook nb;

char* tocstr(std::string const& str) {
	char* cstr = new char[str.size()+1];
	std::copy(str.c_str(), str.c_str()+str.size()+1, cstr);
	return cstr;
}

EMSCRIPTEN_KEEPALIVE
extern "C" Entry* add_ent(const char* name, const char* value, int len) {
//	std::cout<<mpz_class(5)*mpz_class(10)<<std::endl;
//	std::cout<<(mpz_class(5)<<3)<<std::endl;
//	std::cout<<numbits(mpz_class(7))<<std::endl;
//
//	mpz_class x(15);
//	mpz_tdiv_r_2exp(x.get_mpz_t(), x.get_mpz_t(), 2);
//	std::cout<<x<<std::endl;

	return nb.add_entry(name, value, len);
}

EMSCRIPTEN_KEEPALIVE
extern "C" void up_ent(Entry* ent, const char* value, int len) {
	return nb.update_entry(ent, value, len);
}

EMSCRIPTEN_KEEPALIVE
extern "C" void del_ent(Entry* ent) {
	return nb.remove_entry(ent);
}

EMSCRIPTEN_KEEPALIVE
extern "C" bool is_changed(Entry* ent) {
	if (ent->changed) {
		ent->changed=false;
		return true;
	} else {
		return false;
	}
}

EMSCRIPTEN_KEEPALIVE
extern "C" char const* entry_error(Entry* ent) {
	if (ent->parse_error) return ent->parse_error->c_str();
	else if (ent->missing_dep) return ent->missing_dep->what();
	else if (ent->cycle) return "dependency cycle";
	else if (ent->eval_error) return ent->eval_error->c_str();
	else return nullptr;
}

EMSCRIPTEN_KEEPALIVE
extern "C" char* get_tex_input(Entry* ent) {
	std::ostringstream oss;
	ent->ex.tex(oss);
	return tocstr(oss.str());
}

EMSCRIPTEN_KEEPALIVE
extern "C" char* get_output(Entry* ent) {
	std::ostringstream oss;

	RPolynomial& x = ent->output;

	ent->ex.tex(std::cout);
	std::cout<<std::endl;
	std::cout<<x<<std::endl;

	for (int i=0; i<std::min(x.coeffs.size(), ent->len); i++) {
		mpz_class gcd;
		mpz_gcd(gcd.get_mpz_t(), x.coeffs[i].get_mpz_t(), x.den.get_mpz_t());
		mpz_class num = x.coeffs[i]/gcd; mpz_class den=ent->output.den/gcd;
		oss<<num<<"\n"<<den<<"\n";
	}

	return tocstr(oss.str());
}

EMSCRIPTEN_KEEPALIVE
extern "C" void clear_nb() {
	nb = Notebook();
}

#endif

using namespace std::chrono;

int main() {
	time_point tp = high_resolution_clock::now();

	Notebook n;
	n.add_entry("a", "exp(exp(x)-1)", 100);
//	n.add_entry("b", "1/(1-x-i)", 100);

	unsigned ms = duration_cast<milliseconds>(high_resolution_clock::now()-tp).count();
	std::cout<<n.entries[0]->output<<std::endl;
	std::cout<<ms<<" ms"<<std::endl;
}
