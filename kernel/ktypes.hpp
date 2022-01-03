/* ***
 * ktypes.hpp - various types and includes used throughout the kernel
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef KTYPES_HAI
#define KTYPES_HAI

#include <stdint.h>
#include <stddef.h>

#define cast static_cast
#define recast reinterpret_cast

namespace yuki {
template<class T, T v> struct integral_constant {
	static constexpr T value = v;
	using value_type = T;
	using type = integral_constant;
	constexpr operator value_type() const noexcept { return value; }
	constexpr value_type operator()() const noexcept { return value; }
};
typedef integral_constant<bool, false> false_type;
typedef integral_constant<bool, true> true_type;
template<class T> struct remove_reference     { typedef T type; };
template<class T> struct remove_reference<T&> { typedef T type; };
template<class T> struct remove_reference<T&&>{ typedef T type; };
template<class T> using remove_reference_t = typename remove_reference<T>::type;
template<class T> auto test_returnable(int)->
decltype(void(static_cast<T(*)()>(nullptr)), true_type{});
template<class> auto test_returnable(...)->void;
template<class From, class To>
struct is_compatible : integral_constant<bool,
	(decltype(test_returnable<To>(0))::value)
	>{};//>:3
template<class T> constexpr remove_reference_t<T>&& move(T&& t) noexcept {
	return static_cast<typename remove_reference<T>::type&&>(t); }
}//namespace

struct RefCountable {
	uint32_t ref_count;
	void addref() {
		ref_count++;
	}
	void deref() {
		ref_count--;
	}
};

template<class T>
class Ref {
protected:
	T *ref;
public:
	Ref() : ref(nullptr)  {}
	Ref(T *rc) : ref(rc) { rc->addref(); }
	Ref(const Ref &that) : ref(nullptr) { reset(that.ref); }
	Ref(Ref &&that) : ref(that.ref) { that.ref = nullptr; }
	~Ref() { if(ref) { ref->deref(); ref = nullptr; } }
	void reset(T *rc = nullptr) {
		if(rc) rc->addref();
		if(ref) ref->deref();
		ref = rc;
	}
	Ref operator=(const Ref &that) {
		reset(that.ref);
		return *this;
	}
	Ref operator=(Ref &&that) {
		if(ref) ref->deref();
		ref = that.ref;
		that.ref = nullptr;
		return *this;
	}
	bool operator==(const Ref &that) const { return ref == that.ref; }
	operator bool() const { return ref != nullptr; }
	T *operator->() const { return ref; }
	T &operator*() const { return *ref; }
};

int memeq(const void * ptra, const void * ptrb, size_t l);
extern "C" void memset(void * dest, int, size_t l);

#endif

