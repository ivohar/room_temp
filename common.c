/*
    common.c - Common functions for the sensor read utilities
*/
#include <string.h>
#include <iconv.h>
#include <langinfo.h>


char degstr[5];

void set_degstr(void)
{
	const char *deg_default_text = "'C";

	/* Size hardcoded for better performance.
	   Don't forget to count the trailing \0! */
	size_t deg_latin1_size = 3;
	char *deg_latin1_text = "\260C";
	size_t nconv;
	size_t degstr_size = sizeof(degstr);
	char *degstr_ptr = degstr;

	iconv_t cd = iconv_open(nl_langinfo(CODESET), "ISO-8859-1");
	if (cd != (iconv_t) -1) {
		nconv = iconv(cd, &deg_latin1_text, &deg_latin1_size,
			      &degstr_ptr, &degstr_size);
		iconv_close(cd);

		if (nconv != (size_t) -1)
			return;
	}

	/* There was an error during the conversion, use the default text */
	strcpy(degstr, deg_default_text);
}