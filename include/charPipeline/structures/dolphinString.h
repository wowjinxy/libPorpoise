#ifndef	STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C" {
#endif

u8		Strcat	( char *str1, char *str2, char *dst );
void	Strcpy	( char *dst, char *src );
s8		Strcmp	( char *str1, char *str2 );
u32		Strlen	( char *str );

#ifdef __cplusplus
}
#endif

#endif
