/* 
	lib_mysqludf_sys - a library with miscellaneous (operating) system level functions
	Copyright (C) 2007  Roland Bouman 
	Copyright (C) 2008-2009  Roland Bouman and Bernardo Damele A. G.
	web: http://www.mysqludf.org/
	email: mysqludfs@gmail.com, bernardo.damele@gmail.com
	
	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.
	
	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

	05-2019 Updated to mysql 8 by Xavi Meneses 

*/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <mutex>
#include <new>
#include <regex>
#include <string>
#include <vector>

#include "mysql.h"  // IWYU pragma: keep
#include "mysql/udf_registration_types.h"

#ifdef _WIN32
/* inet_aton needs winsock library */
#pragma comment(lib, "ws2_32")
#endif

/*
  Not all platforms have gethostbyaddr_r, so we use a global lock here instead.
  Production-quality code should use getaddrinfo where available.
*/
static std::mutex *LOCK_hostname{nullptr};

/* All function signatures must be right or mysqld will not find the symbol! */

#define MAXMETAPH 8

#define LIBVERSION "lib_mysqludf_sys version 0.0.3"

#ifdef __WIN__
#define SETENV(name,value)              SetEnvironmentVariable(name,value);
#else
#define SETENV(name,value)              setenv(name,value,1);           
#endif

/**
 * lib_mysqludf_sys_info
 */
extern "C" bool lib_mysqludf_sys_info_init( UDF_INIT *initid,UDF_ARGS *args,char *message){
	bool status;
	if(args->arg_count!=0){
		strcpy(
			message
		,	"No arguments allowed (udf: lib_mysqludf_sys_info)"
		);
		status = 1;
	} else {
		status = 0;
	}
	return status;
}
extern "C" void lib_mysqludf_sys_info_deinit(UDF_INIT *initid){}

extern "C" char* lib_mysqludf_sys_info(UDF_INIT *initid,UDF_ARGS *args,char* result,
				       unsigned long *length,unsigned char *is_null,
				       unsigned char *error){
	strcpy(result,LIBVERSION);
	*length = strlen(LIBVERSION);
	return result;
}

extern "C" bool sys_get_init(UDF_INIT *initid,UDF_ARGS *args,char *message){
	if(args->arg_count==1 && args->arg_type[0]==STRING_RESULT){
		initid->maybe_null = 1;
		return 0;
	} else {
		strcpy(
			message
		,	"Expected exactly one string type parameter"
		);		
		return 1;
	}
}

extern "C" void sys_get_deinit(UDF_INIT *initid){}

extern "C" char* sys_get(UDF_INIT *initid,UDF_ARGS *args,char* result,
               	         unsigned long* length,unsigned char *is_null,
		  	 unsigned char *error){
	char* value = getenv(args->args[0]);
	if(value == NULL){
		*is_null = 1;
	} else {
		*length = strlen(value);
	} 
	return value;
}

extern "C" bool sys_set_init(UDF_INIT *initid,UDF_ARGS *args,char *message){
	if(args->arg_count!=2){
		strcpy(
			message
		,	"Expected exactly two arguments"
		);		
		return 1;
	}
	if(args->arg_type[0]!=STRING_RESULT){
		strcpy(
			message
		,	"Expected string type for name parameter"
		);		
		return 1;
	}
	args->arg_type[1]=STRING_RESULT;
	if((initid->ptr=(char*)malloc(
		args->lengths[0]
	+	1
	+	args->lengths[1]
	+	1
	))==NULL){
		strcpy(
			message
		,	"Could not allocate memory"
		);		
		return 1;
	}	
	return 0;
}

extern "C" void sys_set_deinit(UDF_INIT *initid){
	if (initid->ptr!=NULL){
		free(initid->ptr);
	}
}

extern "C" long long sys_set(UDF_INIT *initid,UDF_ARGS *args,char* result,
                         unsigned long* length,unsigned char *is_null,
                         unsigned char *error){
	char *name = initid->ptr;
	char *value = name + args->lengths[0] + 1; 
	memcpy(
		name
	,	args->args[0]
	,	args->lengths[0]
	);
	*(name + args->lengths[0]) = '\0';
	memcpy(
		value
	,	args->args[1]
	,	args->lengths[1]
	);
	*(value + args->lengths[1]) = '\0';
	return SETENV(name,value);		
}

extern "C" bool sys_exec_init(UDF_INIT *initid,UDF_ARGS *args,char *message){
	unsigned int i=0;
	if(args->arg_count == 1
	&& args->arg_type[i]==STRING_RESULT){
		return 0;
	} else {
		strcpy(message,"Expected exactly one string type parameter");		
		return 1;
	}
}
extern "C" void sys_exec_deinit( UDF_INIT *initid){}

extern "C" long long  sys_exec(UDF_INIT *initid,UDF_ARGS *args,char* result,
                         unsigned long* length,unsigned char *is_null,
                         unsigned char *error){
	return system(args->args[0]);
}

extern "C" bool sys_eval_init(UDF_INIT *initid,UDF_ARGS *args,char *message){
	unsigned int i=0;
	if(args->arg_count == 1
	&& args->arg_type[i]==STRING_RESULT){
		return 0;
	} else {
		strcpy(message,"Expected exactly one string type parameter");		
		return 1;
	}
}
extern "C" void sys_eval_deinit(UDF_INIT *initid){
}

extern "C" char* sys_eval(UDF_INIT *initid,UDF_ARGS *args,char* result,
                         unsigned long* length,unsigned char *is_null,
                         unsigned char *error){
	FILE *pipe;
	char line[1024];
	unsigned long outlen, linelen;

	result = (char*)malloc(1);
	outlen = 0;

	pipe = popen(args->args[0], "r");

	while (fgets(line, sizeof(line), pipe) != NULL) {
		linelen = strlen(line);
		result = (char*)realloc(result, outlen + linelen);
		strncpy(result + outlen, line, linelen);
		outlen = outlen + linelen;
	}

	pclose(pipe);

	if (!(*result) || result == NULL) {
		*is_null = 1;
	} else {
		result[outlen] = 0x00;
		*length = strlen(result);
	}

	return result;
}


