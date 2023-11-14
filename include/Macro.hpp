
#define NONCOPYABLE(TypeName)\
    TypeName(const TypeName& other) = delete;\
    TypeName& operator=(TypeName& other) = delete;

#define NONMOVABLE(TypeName)\
    TypeName(const TypeName&& other) = delete;\
    TypeName&& operator=(TypeName&& other) = delete;