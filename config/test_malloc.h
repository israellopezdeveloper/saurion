 
#ifndef TEST_MALLOC_H 
#define TEST_MALLOC_H 

#include <stddef.h> 
#include <stdlib.h> 
#include <stdatomic.h> 

// Definición de las variables globales 
static volatile atomic_int test_mode; 
static volatile atomic_int num_mallocs; 
static volatile atomic_int target_mallocs; 
atomic_store(&test_mode, 0); 
atomic_store(&num_mallocs, 0); 
atomic_store(&target_mallocs, 0); 

void *fake_malloc(unsigned long size) { 
	void *ptr = NULL; 
	printf("fake_malloc atomic_load(&test_mode)=%d, atomic_load(&num_mallocs)=%d, atomic_load(&target_mallocs)=%d", atomic_load(&test_mode), atomic_load(&num_mallocs), atomic_load(&target_mallocs)); 
	puts("");
	if (atomic_load(&test_mode) == 0) { 
		ptr = malloc(size); 
	} else if (atomic_load(&test_mode) == 1 && atomic_load(&num_mallocs) == atomic_load(&target_mallocs)) { 
	  puts("test mode 1"); 
		ptr = NULL; 
	} else if (atomic_load(&test_mode) == 2 && atomic_load(&num_mallocs) < atomic_load(&target_mallocs)) { 
	  puts("test mode 2"); 
		ptr = NULL; 
	} else if (atomic_load(&test_mode) == 3 && atomic_load(&num_mallocs) > atomic_load(&target_mallocs)) { 
	  puts("test mode 3"); 
		ptr = NULL; 
	} else { 
		puts("test mode else"); 
		ptr = malloc(size); 
	} 
	if (ptr != NULL || atomic_load(&test_mode) != 0) { 
		atomic_load(&num_mallocs)++; 
	} 
	return ptr; 
} 

// Macro para manejar las diferentes condiciones de test_mode 
#define malloc(size) fake_malloc(size) 

#endif  // !TEST_MALLOC_H 
