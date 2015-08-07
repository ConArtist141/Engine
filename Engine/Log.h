#ifdef _DEBUG
#define LOG(x) OutputDebugString("Renderer: "x"\n")
#define PRINT(x) OutputDebugString(x)
#else
#define LOG(x)
#endif