#define HOST2LE32(x) (x)
#define LE2HOST32(x) (x)

#define HOST2LE16(x) (x)
#define LE2HOST16(x) (x)

#define GETLEs16(X) ((s16)GETLE16((u16 *)X))
#define GETLEs32(X) ((s16)GETLE32((u16 *)X))

#define GETLE16(X) LE2HOST16(*(u16 *)X)
#define GETLE32(X) LE2HOST32(*(u32 *)X)

#define PUTLE16(X, Y)   *((u16 *)X) = (u16)Y;
#define PUTLE32(X, Y)   *((u32 *)X) = (u32)Y;
