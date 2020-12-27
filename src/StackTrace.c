/*******************************************************************************
 * Copyright (c) 2009, 2020 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "StackTrace.h"
#include "Log.h"
#include "LinkedList.h"

#include "Clients.h"
#include "Thread.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#define snprintf _snprintf
#endif

/*BE
def STACKENTRY
{
	n32 ptr STRING open "name"
	n32 dec "line"
}

defList(STACKENTRY)
BE*/

#define MAX_STACK_DEPTH 50
#define MAX_FUNCTION_NAME_LENGTH 30
#define MAX_THREADS 255

typedef struct
{
	thread_id_type threadid;
	char name[MAX_FUNCTION_NAME_LENGTH];
	int line;
} stackEntry;

typedef struct
{
	thread_id_type id;
	int maxdepth;
	int current_depth;			// comment by Clark:: 同一个线程调用的深度         ::2020-12-26
	stackEntry callstack[MAX_STACK_DEPTH];
} threadEntry;

#include "StackTrace.h"

#if !defined(NOSTACKTRACE)

static int thread_count = 0;
static threadEntry threads[MAX_THREADS];
static threadEntry *my_thread = NULL;

#if defined(_WIN32) || defined(_WIN64)
mutex_type stack_mutex;
#else
static pthread_mutex_t stack_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static mutex_type stack_mutex = &stack_mutex_store;
#endif


int setStack(int create);


int setStack(int create)
{
	int i = -1;
	thread_id_type curid = Thread_getid();

	my_thread = NULL;
	for (i = 0; i < MAX_THREADS && i < thread_count; ++i)   // comment by Clark:: 当前有多少线程  ::2020-12-26
	{
		if (threads[i].id == curid)								// comment by Clark::  存在的话，就从数组中以出当前线程的相关打印信息                         ::2020-12-26
		{
			my_thread = &threads[i];
			break;
		}
	}

	if (my_thread == NULL && create && thread_count < MAX_THREADS)			// comment by Clark:: 若没有且指定创建, 则占用一个, 并初始化自身   ::2020-12-26
	{
		my_thread = &threads[thread_count];
		my_thread->id = curid;
		my_thread->maxdepth = 0;
		my_thread->current_depth = 0;
		++thread_count;
	}
	return my_thread != NULL; /* good == 1 */
}

void StackTrace_entry(const char* name, int line, enum LOG_LEVELS trace_level)
{
	Thread_lock_mutex(stack_mutex);
	if (!setStack(1))
		goto exit;
	if (trace_level != -1)// comment by Clark:: 这个是实时打印的  ::2020-12-26
		Log_stackTrace(trace_level, 9, (int)my_thread->id, my_thread->current_depth, name, line, NULL);

	// comment by Clark:: 这个是备份, 为回溯做准备的  ::2020-12-26
	strncpy(my_thread->callstack[my_thread->current_depth].name, name, sizeof(my_thread->callstack[0].name)-1);
	my_thread->callstack[(my_thread->current_depth)++].line = line;
	if (my_thread->current_depth > my_thread->maxdepth)			// comment by Clark:: 一个线程当前的打印深度, 超过以往的打印最深度, 则重新刷新 达到过的maxdepth  ::2020-12-26
		my_thread->maxdepth = my_thread->current_depth;
	if (my_thread->current_depth >= MAX_STACK_DEPTH)			// comment by Clark:: 当线程达到的最深度超过了最大的深度时，则报错  ::2020-12-26
		Log(LOG_FATAL, -1, "Max stack depth exceeded");
exit:
	Thread_unlock_mutex(stack_mutex);
}


void StackTrace_exit(const char* name, int line, void* rc, enum LOG_LEVELS trace_level)
{
	Thread_lock_mutex(stack_mutex);
	if (!setStack(0))
		goto exit;
	if (--(my_thread->current_depth) < 0)
		Log(LOG_FATAL, -1, "Minimum stack depth exceeded for thread %lu", my_thread->id);
	// comment by Clark:: 同一个函数有进就会有出, 所以 entry 与 exit 要成对使用   ::2020-12-26
	if (strncmp(my_thread->callstack[my_thread->current_depth].name, name, sizeof(my_thread->callstack[0].name)-1) != 0)  
		Log(LOG_FATAL, -1, "Stack mismatch. Entry:%s Exit:%s\n", my_thread->callstack[my_thread->current_depth].name, name);
	if (trace_level != -1)
	{
		if (rc == NULL)
			Log_stackTrace(trace_level, 10, (int)my_thread->id, my_thread->current_depth, name, line, NULL);
		else
			Log_stackTrace(trace_level, 11, (int)my_thread->id, my_thread->current_depth, name, line, (int*)rc);
	}
exit:
	Thread_unlock_mutex(stack_mutex);
}

// comment by Clark:: 打印所有线程的所有回溯  ::2020-12-26
void StackTrace_printStack(FILE* dest)
{
	FILE* file = stdout;
	int t = 0;

	if (dest)
		file = dest;
	for (t = 0; t < thread_count; ++t)
	{
		threadEntry *cur_thread = &threads[t];

		if (cur_thread->id > 0)
		{
			int i = cur_thread->current_depth - 1;

			fprintf(file, "=========== Start of stack trace for thread %lu ==========\n", (unsigned long)cur_thread->id);
			if (i >= 0)
			{
				fprintf(file, "%s (%d)\n", cur_thread->callstack[i].name, cur_thread->callstack[i].line);
				while (--i >= 0)
					fprintf(file, "   at %s (%d)\n", cur_thread->callstack[i].name, cur_thread->callstack[i].line);
			}
			fprintf(file, "=========== End of stack trace for thread %lu ==========\n\n", (unsigned long)cur_thread->id);
		}
	}
	if (file != stdout && file != stderr && file != NULL)
		fclose(file);
}


char* StackTrace_get(thread_id_type threadid, char* buf, int bufsize)
{
	int t = 0;

	if (bufsize < 100)
		goto exit;
	buf[0] = '\0';
	for (t = 0; t < thread_count; ++t)
	{
		threadEntry *cur_thread = &threads[t];

		if (cur_thread->id == threadid)
		{
			int i = cur_thread->current_depth - 1;
			int curpos = 0;

			if (i >= 0)
			{
				curpos += snprintf(&buf[curpos], bufsize - curpos -1,
						"%s (%d)\n", cur_thread->callstack[i].name, cur_thread->callstack[i].line);
				while (--i >= 0)
					curpos += snprintf(&buf[curpos], bufsize - curpos -1, /* lgtm [cpp/overflowing-snprintf] */
							"   at %s (%d)\n", cur_thread->callstack[i].name, cur_thread->callstack[i].line);
				if (buf[--curpos] == '\n')
					buf[curpos] = '\0';
			}
			break;
		}
	}
exit:
	return buf;
}

#endif
