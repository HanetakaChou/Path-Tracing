#define BYTE uint8_t
#ifndef NDEBUG
#include "debug/_internal_skin_compute.inl"
#else
#include "release/_internal_skin_compute.inl"
#endif
#undef BYTE