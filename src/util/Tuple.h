#ifndef Tuple_H
#define Tuple_H
#include <iostream>
#include <string>
#include <functional>
#include <limits.h>
#include <bitset>

inline std::ostream &operator<<(std::ostream &os, unsigned char c) {
    return os << static_cast<unsigned int> (c);
}

inline std::istream &operator>>(std::istream &is, unsigned char& c) {
    unsigned short i;
    is >> i;
    c = i;
    return is;
}

typedef std::bitset<33> col_type;

template<typename... T> struct ValTuple {
};

template<typename... T> struct KeyTuple {
};

template<typename T1>
struct ValTuple<T1> {
    T1 _1;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1>));
    }

    ValTuple(T1 t1) {
        _1 = t1;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
    }

    void copyColsFrom(const ValTuple<T1>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
    }

    bool operator==(const ValTuple<T1>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ")";
        return s;
    }

};

template<typename T1>
struct KeyTuple<T1> {
    T1 _1;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1>));
    }

    KeyTuple(T1 t1) {
        _1 = t1;
    }

    bool operator==(const KeyTuple<T1>& that) const {
        return _1 == that._1;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1> const& obj) {
        s << "(" << obj._1 << ")";
        return s;
    }

};

template<typename T1, typename T2>
struct ValTuple<T1, T2> {
    T1 _1;
    T2 _2;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2>));
    }

    ValTuple(T1 t1, T2 t2) {
        _1 = t1;
        _2 = t2;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
    }

    void copyColsFrom(const ValTuple<T1, T2>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
    }

    bool operator==(const ValTuple<T1, T2>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ")";
        return s;
    }

};

template<typename T1, typename T2>
struct KeyTuple<T1, T2> {
    T1 _1;
    T2 _2;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2>));
    }

    KeyTuple(T1 t1, T2 t2) {
        _1 = t1;
        _2 = t2;
    }

    bool operator==(const KeyTuple<T1, T2>& that) const {
        return _1 == that._1 && _2 == that._2;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3>
struct ValTuple<T1, T2, T3> {
    T1 _1;
    T2 _2;
    T3 _3;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
    }

    bool operator==(const ValTuple<T1, T2, T3>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3>
struct KeyTuple<T1, T2, T3> {
    T1 _1;
    T2 _2;
    T3 _3;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
    }

    bool operator==(const KeyTuple<T1, T2, T3>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4>
struct ValTuple<T1, T2, T3, T4> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4>
struct KeyTuple<T1, T2, T3, T4> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5>
struct ValTuple<T1, T2, T3, T4, T5> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5>
struct KeyTuple<T1, T2, T3, T4, T5> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct ValTuple<T1, T2, T3, T4, T5, T6> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct KeyTuple<T1, T2, T3, T4, T5, T6> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
        if (!cols[9]) _9 = that._9;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
        if (cols[9]) _9 = that._9;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
        if (!cols[9]) _9 = that._9;
        if (!cols[10]) _10 = that._10;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
        if (cols[9]) _9 = that._9;
        if (cols[10]) _10 = that._10;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
        if (!cols[9]) _9 = that._9;
        if (!cols[10]) _10 = that._10;
        if (!cols[11]) _11 = that._11;
        if (!cols[12]) _12 = that._12;
        if (!cols[13]) _13 = that._13;
        if (!cols[14]) _14 = that._14;
        if (!cols[15]) _15 = that._15;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
        if (cols[9]) _9 = that._9;
        if (cols[10]) _10 = that._10;
        if (cols[11]) _11 = that._11;
        if (cols[12]) _12 = that._12;
        if (cols[13]) _13 = that._13;
        if (cols[14]) _14 = that._14;
        if (cols[15]) _15 = that._15;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    T16 _16;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15, T16 t16) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        _16 = t16;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
        if (!cols[9]) _9 = that._9;
        if (!cols[10]) _10 = that._10;
        if (!cols[11]) _11 = that._11;
        if (!cols[12]) _12 = that._12;
        if (!cols[13]) _13 = that._13;
        if (!cols[14]) _14 = that._14;
        if (!cols[15]) _15 = that._15;
        if (!cols[16]) _16 = that._16;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
        if (cols[9]) _9 = that._9;
        if (cols[10]) _10 = that._10;
        if (cols[11]) _11 = that._11;
        if (cols[12]) _12 = that._12;
        if (cols[13]) _13 = that._13;
        if (cols[14]) _14 = that._14;
        if (cols[15]) _15 = that._15;
        if (cols[16]) _16 = that._16;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15 && _16 == that._16;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ", " << obj._16 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    T16 _16;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15, T16 t16) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        _16 = t16;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15 && _16 == that._16;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ", " << obj._16 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16, typename T17, typename T18>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    T16 _16;
    T17 _17;
    T18 _18;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15, T16 t16, T17 t17, T18 t18) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        _16 = t16;
        _17 = t17;
        _18 = t18;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
        if (!cols[9]) _9 = that._9;
        if (!cols[10]) _10 = that._10;
        if (!cols[11]) _11 = that._11;
        if (!cols[12]) _12 = that._12;
        if (!cols[13]) _13 = that._13;
        if (!cols[14]) _14 = that._14;
        if (!cols[15]) _15 = that._15;
        if (!cols[16]) _16 = that._16;
        if (!cols[17]) _17 = that._17;
        if (!cols[18]) _18 = that._18;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
        if (cols[9]) _9 = that._9;
        if (cols[10]) _10 = that._10;
        if (cols[11]) _11 = that._11;
        if (cols[12]) _12 = that._12;
        if (cols[13]) _13 = that._13;
        if (cols[14]) _14 = that._14;
        if (cols[15]) _15 = that._15;
        if (cols[16]) _16 = that._16;
        if (cols[17]) _17 = that._17;
        if (cols[18]) _18 = that._18;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15 && _16 == that._16 && _17 == that._17 && _18 == that._18;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ", " << obj._16 << ", " << obj._17 << ", " << obj._18 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16, typename T17, typename T18>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    T16 _16;
    T17 _17;
    T18 _18;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15, T16 t16, T17 t17, T18 t18) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        _16 = t16;
        _17 = t17;
        _18 = t18;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15 && _16 == that._16 && _17 == that._17 && _18 == that._18;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ", " << obj._16 << ", " << obj._17 << ", " << obj._18 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16, typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24, typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32>
struct ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    T16 _16;
    T17 _17;
    T18 _18;
    T19 _19;
    T20 _20;
    T21 _21;
    T22 _22;
    T23 _23;
    T24 _24;
    T25 _25;
    T26 _26;
    T27 _27;
    T28 _28;
    T29 _29;
    T30 _30;
    T31 _31;
    T32 _32;
    bool isNotNull;

    ValTuple() {
        memset(this, 0, sizeof (ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32>));
    }

    ValTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15, T16 t16, T17 t17, T18 t18, T19 t19, T20 t20, T21 t21, T22 t22, T23 t23, T24 t24, T25 t25, T26 t26, T27 t27, T28 t28, T29 t29, T30 t30, T31 t31, T32 t32) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        _16 = t16;
        _17 = t17;
        _18 = t18;
        _19 = t19;
        _20 = t20;
        _21 = t21;
        _22 = t22;
        _23 = t23;
        _24 = t24;
        _25 = t25;
        _26 = t26;
        _27 = t27;
        _28 = t28;
        _29 = t29;
        _30 = t30;
        _31 = t31;
        _32 = t32;
        isNotNull = true;
    }

    void copyColsFromExcept(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32>& that, const col_type& cols) {
        if (!cols[0]) isNotNull = that.isNotNull;
        if (!cols[1]) _1 = that._1;
        if (!cols[2]) _2 = that._2;
        if (!cols[3]) _3 = that._3;
        if (!cols[4]) _4 = that._4;
        if (!cols[5]) _5 = that._5;
        if (!cols[6]) _6 = that._6;
        if (!cols[7]) _7 = that._7;
        if (!cols[8]) _8 = that._8;
        if (!cols[9]) _9 = that._9;
        if (!cols[10]) _10 = that._10;
        if (!cols[11]) _11 = that._11;
        if (!cols[12]) _12 = that._12;
        if (!cols[13]) _13 = that._13;
        if (!cols[14]) _14 = that._14;
        if (!cols[15]) _15 = that._15;
        if (!cols[16]) _16 = that._16;
        if (!cols[17]) _17 = that._17;
        if (!cols[18]) _18 = that._18;
        if (!cols[19]) _19 = that._19;
        if (!cols[20]) _20 = that._20;
        if (!cols[21]) _21 = that._21;
        if (!cols[22]) _22 = that._22;
        if (!cols[23]) _23 = that._23;
        if (!cols[24]) _24 = that._24;
        if (!cols[25]) _25 = that._25;
        if (!cols[26]) _26 = that._26;
        if (!cols[27]) _27 = that._27;
        if (!cols[28]) _28 = that._28;
        if (!cols[29]) _29 = that._29;
        if (!cols[30]) _30 = that._30;
        if (!cols[31]) _31 = that._31;
        if (!cols[32]) _32 = that._32;
    }

    void copyColsFrom(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32>& that, const col_type& cols) {
        if (cols[0]) isNotNull = that.isNotNull;
        if (cols[1]) _1 = that._1;
        if (cols[2]) _2 = that._2;
        if (cols[3]) _3 = that._3;
        if (cols[4]) _4 = that._4;
        if (cols[5]) _5 = that._5;
        if (cols[6]) _6 = that._6;
        if (cols[7]) _7 = that._7;
        if (cols[8]) _8 = that._8;
        if (cols[9]) _9 = that._9;
        if (cols[10]) _10 = that._10;
        if (cols[11]) _11 = that._11;
        if (cols[12]) _12 = that._12;
        if (cols[13]) _13 = that._13;
        if (cols[14]) _14 = that._14;
        if (cols[15]) _15 = that._15;
        if (cols[16]) _16 = that._16;
        if (cols[17]) _17 = that._17;
        if (cols[18]) _18 = that._18;
        if (cols[19]) _19 = that._19;
        if (cols[20]) _20 = that._20;
        if (cols[21]) _21 = that._21;
        if (cols[22]) _22 = that._22;
        if (cols[23]) _23 = that._23;
        if (cols[24]) _24 = that._24;
        if (cols[25]) _25 = that._25;
        if (cols[26]) _26 = that._26;
        if (cols[27]) _27 = that._27;
        if (cols[28]) _28 = that._28;
        if (cols[29]) _29 = that._29;
        if (cols[30]) _30 = that._30;
        if (cols[31]) _31 = that._31;
        if (cols[32]) _32 = that._32;
    }

    bool operator==(const ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32>& that) const {
        if (!(isNotNull && that.isNotNull)) return !(isNotNull || that.isNotNull);
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15 && _16 == that._16 && _17 == that._17 && _18 == that._18 && _19 == that._19 && _20 == that._20 && _21 == that._21 && _22 == that._22 && _23 == that._23 && _24 == that._24 && _25 == that._25 && _26 == that._26 && _27 == that._27 && _28 == that._28 && _29 == that._29 && _30 == that._30 && _31 == that._31 && _32 == that._32;
    }

    friend std::ostream& operator<<(std::ostream& s, ValTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32> const& obj) {
        if (!obj.isNotNull) s << "NULL";
        else s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ", " << obj._16 << ", " << obj._17 << ", " << obj._18 << ", " << obj._19 << ", " << obj._20 << ", " << obj._21 << ", " << obj._22 << ", " << obj._23 << ", " << obj._24 << ", " << obj._25 << ", " << obj._26 << ", " << obj._27 << ", " << obj._28 << ", " << obj._29 << ", " << obj._30 << ", " << obj._31 << ", " << obj._32 << ")";
        return s;
    }

};

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16, typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24, typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32>
struct KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32> {
    T1 _1;
    T2 _2;
    T3 _3;
    T4 _4;
    T5 _5;
    T6 _6;
    T7 _7;
    T8 _8;
    T9 _9;
    T10 _10;
    T11 _11;
    T12 _12;
    T13 _13;
    T14 _14;
    T15 _15;
    T16 _16;
    T17 _17;
    T18 _18;
    T19 _19;
    T20 _20;
    T21 _21;
    T22 _22;
    T23 _23;
    T24 _24;
    T25 _25;
    T26 _26;
    T27 _27;
    T28 _28;
    T29 _29;
    T30 _30;
    T31 _31;
    T32 _32;

    KeyTuple() {
        memset(this, 0, sizeof (KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32>));
    }

    KeyTuple(T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15, T16 t16, T17 t17, T18 t18, T19 t19, T20 t20, T21 t21, T22 t22, T23 t23, T24 t24, T25 t25, T26 t26, T27 t27, T28 t28, T29 t29, T30 t30, T31 t31, T32 t32) {
        _1 = t1;
        _2 = t2;
        _3 = t3;
        _4 = t4;
        _5 = t5;
        _6 = t6;
        _7 = t7;
        _8 = t8;
        _9 = t9;
        _10 = t10;
        _11 = t11;
        _12 = t12;
        _13 = t13;
        _14 = t14;
        _15 = t15;
        _16 = t16;
        _17 = t17;
        _18 = t18;
        _19 = t19;
        _20 = t20;
        _21 = t21;
        _22 = t22;
        _23 = t23;
        _24 = t24;
        _25 = t25;
        _26 = t26;
        _27 = t27;
        _28 = t28;
        _29 = t29;
        _30 = t30;
        _31 = t31;
        _32 = t32;
    }

    bool operator==(const KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32>& that) const {
        return _1 == that._1 && _2 == that._2 && _3 == that._3 && _4 == that._4 && _5 == that._5 && _6 == that._6 && _7 == that._7 && _8 == that._8 && _9 == that._9 && _10 == that._10 && _11 == that._11 && _12 == that._12 && _13 == that._13 && _14 == that._14 && _15 == that._15 && _16 == that._16 && _17 == that._17 && _18 == that._18 && _19 == that._19 && _20 == that._20 && _21 == that._21 && _22 == that._22 && _23 == that._23 && _24 == that._24 && _25 == that._25 && _26 == that._26 && _27 == that._27 && _28 == that._28 && _29 == that._29 && _30 == that._30 && _31 == that._31 && _32 == that._32;
    }

    friend std::ostream& operator<<(std::ostream& s, KeyTuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29, T30, T31, T32> const& obj) {
        s << "(" << obj._1 << ", " << obj._2 << ", " << obj._3 << ", " << obj._4 << ", " << obj._5 << ", " << obj._6 << ", " << obj._7 << ", " << obj._8 << ", " << obj._9 << ", " << obj._10 << ", " << obj._11 << ", " << obj._12 << ", " << obj._13 << ", " << obj._14 << ", " << obj._15 << ", " << obj._16 << ", " << obj._17 << ", " << obj._18 << ", " << obj._19 << ", " << obj._20 << ", " << obj._21 << ", " << obj._22 << ", " << obj._23 << ", " << obj._24 << ", " << obj._25 << ", " << obj._26 << ", " << obj._27 << ", " << obj._28 << ", " << obj._29 << ", " << obj._30 << ", " << obj._31 << ", " << obj._32 << ")";
        return s;
    }

};

#endif /* Tuple_H */
