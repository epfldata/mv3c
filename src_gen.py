
numTuples=10
otherTuples=[15, 16, 18, 32]
print "#ifndef Tuple_H"
print "#define	Tuple_H"
print "#include <iostream>"
print "#include <string>"		
print "#include <functional>"
print "#include <limits.h>"
print "#include <bitset>"
print " "
print "inline std::ostream &operator<<(std::ostream &os, unsigned char c) {"
print "    return os << static_cast<unsigned int> (c);"
print "}"
print ""
print "inline std::istream &operator>>(std::istream &is, unsigned char& c) {"
print "    unsigned short i;"
print "    is >> i;"
print "    c = i;"
print "    return is;"
print "}"
print " "
print "typedef std::bitset<33> col_type;"
print " "
print "template<typename... T> struct ValTuple {}; "
print "template<typename... T> struct KeyTuple {}; "
for i in range(1,numTuples+1)+otherTuples:
  cols = range(1, i+1)
  valtype="ValTuple<" + ", ".join(map(lambda x: "T"+str(x), cols))+">"
  keytype="KeyTuple<" + ", ".join(map(lambda x: "T"+str(x), cols))+">"
  template = "template<" + ", ".join(map(lambda x: "typename T"+str(x), cols))+">"
  
  print template
  print "struct "+valtype+" {"
  for j in cols:
    print "  T"+str(j)+" _"+str(j)+";"
  
  print "  bool isNotNull;"
  print "  "

  
  print "  ValTuple() {  memset(this, 0, sizeof("+valtype+")); } "

  print "  ValTuple("+ ", ".join(map(lambda x: "T"+str(x)+" t"+str(x), cols)) + ") {"
  for j in cols:
    print "    _"+str(j)+" = t"+str(j)+";"
  print "    isNotNull=true;   "
  print "  }"
  

  print "  void copyColsFromExcept(const "+valtype+"& that, const col_type& cols){"
  print "    if(!cols[0]) isNotNull = that.isNotNull;"
  for j in cols:
    print "    if(!cols["+str(j)+"]) _"+str(j)+" = that._"+str(j)+";"
  print "  }"
  print " "

  print "  void copyColsFrom(const "+valtype+"& that, const col_type& cols){"
  print "    if(cols[0]) isNotNull = that.isNotNull;"
  for j in cols:
    print "    if(cols["+str(j)+"]) _"+str(j)+" = that._"+str(j)+";"
  print "  }"
  print " "

  print "  bool operator ==(const "+valtype+"& that) const { "
  print "    if(!(isNotNull && that.isNotNull))  return !(isNotNull || that.isNotNull);"
  print "    return "+" && ".join(map(lambda x: "_"+str(x)+" == that._"+str(x), cols)) +";"
  print "  }"
  print "  "

  print "  friend std::ostream& operator<<(std::ostream& s, "+valtype+" const& obj) {"
  print "    if(!obj.isNotNull) s<<\"NULL\";"
  print "    else s << \"(\" <<"+ " << \", \" << ".join(map(lambda x: "obj._"+str(x), cols))+ " << \")\";"
  print " return s;"
  print "  }"  
  print "  "
  print "};\n" 


  print template
  print "struct "+keytype+" {"
  for j in cols:
    print "  T"+str(j)+" _"+str(j)+";"
  
  
  print "  KeyTuple() {  memset(this, 0, sizeof("+keytype+")); } "

  print "  KeyTuple("+ ", ".join(map(lambda x: "T"+str(x)+" t"+str(x), cols)) + ") {"
  for j in cols:
    print "    _"+str(j)+" = t"+str(j)+";"
  print "  }"

  print "  bool operator ==(const "+keytype+"& that) const { "
  print "    return "+" && ".join(map(lambda x: "_"+str(x)+" == that._"+str(x), cols)) +";"
  print "  }"
  print "  "

  print "  friend std::ostream& operator<<(std::ostream& s, "+keytype+" const& obj) {"
  print "    s << \"(\" <<"+ " << \", \" << ".join(map(lambda x: "obj._"+str(x), cols))+ " << \")\";"
  print "    return s;"
  print "  }"  
  print "  "
  print "};\n" 
"""
  print template
  print "struct Hasher<"+valtype+"> {"
  for j in cols:
    print "  static std::hash<T"+str(j)+"> h"+str(j)+";"
  print "  "

  print "  inline size_t operator()(const "+valtype+"& v) const {"
  if i==1:
    print "    return h1(v._1);"
  else:
    print "    const unsigned int mask = (CHAR_BIT*sizeof(size_t)-1);"
    print "    size_t hash = 0xcafebabe;"
    print "    size_t mix = h1(v._1)*0xcc9e2d51;"
    print "    mix = (mix << 15) | (mix >> ((-15)&mask)); "
    print "    mix = (mix * 0x1b873593) ^ hash;"
    print "    mix = (mix << 13) | (mix >> ((-13)&mask));"
    print "    hash = (mix << 1) + mix + 0xe6546b64;"
    for j in range(2,i+1):
      print "    mix = h"+str(j)+"(v._"+str(j)+")*0xcc9e2d51;"
      print "    mix = (mix << 15) | (mix >> ((-15)&mask)); "
      print "    mix = (mix * 0x1b873593) ^ hash;"
      print "    mix = (mix << 13) | (mix >> ((-13)&mask));"
      print "    hash = (mix << 1) + mix + 0xe6546b64;"
    print "    hash ^=  "+str(i)+";"
    print "    hash ^=  (hash >> 16);"
    print "    hash *=  0x85ebca6b;"
    print "    hash ^=  (hash >> 13);"
    print "    hash *=   0xc2b2ae35;"
    print "    hash ^= (hash>>16);"	
    print "    return hash;"
  print "  }"
  print "}; "

  for k in cols:
    print "template<typename T1",
    for j in range(2,i+1):
      print (", typename T"+str(j)),
    print ">"
    print "std::hash<T"+str(k)+"> Hasher<"+valtype+">::h"+str(k)+";"
  print " \n"
  """
print "#endif	/* Tuple_H */"
