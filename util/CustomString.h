#ifndef STRING_H
#define STRING_H
#include <cstring>

template <int N>
class String {
public:
    char data[N + 1];

    String(const char* d) {
        memset(data, 0, N + 1);
        memcpy(data, d, N);

    }

    //    String(const String<N>& that) {
    //        memcpy(data, that.data, N);
    //        data[N] = 0;
    //    }

    String() {
         memset(data, 0, N + 1);
    }

    const char* c_str() const {
        return data;
    }

    //    const String<N>& operator=(const String<N>& that) {
    //        memcpy(data, that.data, N);
    //        data[N] = 0;
    //        return *this;
    //    }

    bool operator==(const String<N>& that) const {
        return strcmp(data, that.data) == 0;
    }

    bool contains(const char* substr) const {
        return strcmp(data, substr) == 0; //TODO FIX
    }

    void insertAt(int pos, const char* STR, int maxchars) {
        strncpy(data + pos, STR, maxchars);
        assert(pos + maxchars <= N);
        data[pos + maxchars] = 0;
    }
    //    bool operator < (const String<N>& that) const {
    //        return strcmp(data, that.data)<0;
    //    }

    bool operator<(const String<N>& that) const {
        for (int i = 0; i < N && that.data[i]; i++) {
            char c1 = tolower(data[i]);
            char c2 = tolower(that.data[i]);
            if (c1 == c2)
                continue;
            else if (c1 < c2)
                return true;
            else return false;
        }
        return false;
    }

    friend std::ostream& operator<<(std::ostream& s, const String<N>& obj) {
        s << obj.data;
        return s;
    }

    friend std::istream& operator>>(std::istream& s, String<N>& obj) {
        s >> obj.data;
        obj.data[N] = 0;
        return s;
    }
};
namespace std {
    template <int N>
    struct hash<String<N>>
    {

        inline size_t operator()(const String<N>& x) const {
            size_t hash = 7;
            for (int i = 0; i < (N < 100 ? N : 100) && x.data[i]; i++) {
                hash = hash * 31 + x.data[i];
            }
            return hash;
        }
    };

}
#endif
