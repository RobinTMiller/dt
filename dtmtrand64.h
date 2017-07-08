#if !defined(dtmtrand64_h)
#define dtmtrand64_h 1

/* Note: These should have more meaningful and unique names (IMHO). */
#define NN 312
#define MM 156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define LM 0x7FFFFFFFULL /* Least significant 31 bits */

/* This is for per thread specific random number generator. */
typedef struct mtrand64 {
    uint64_t mt[NN];		/* The state vector array.	*/
    int	mti;			/* The state vector index.	*/
} mtrand64_t;

#endif /* !defined(dtmtrand64_h) */
