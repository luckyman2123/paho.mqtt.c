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
	int current_depth;			// comment by Clark:: ͬһ���̵߳��õ����         ::2020-12-26
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
	for (i = 0; i < MAX_THREADS && i < thread_count; ++i)   // comment by Clark:: ��ǰ�ж����߳�  ::2020-12-26
	{
		if (threads[i].id == curid)								// comment by Clark::  ���ڵĻ����ʹ��������Գ���ǰ�̵߳���ش�ӡ��Ϣ                         ::2020-12-26
		{
			my_thread = &threads[i];
			break;
		}
	}

	if (my_thread == NULL && create && thread_count < MAX_THREADS)			// comment by Clark:: ��û����ָ������, ��ռ��һ��, ����ʼ������   ::2020-12-26
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
	if (trace_level != -1)// comment by Clark:: �����ʵʱ��ӡ��  ::2020-12-26
		Log_stackTrace(trace_level, 9, (int)my_thread->id, my_thread->current_depth, name, line, NULL);

	// comment by Clark:: ����Ǳ���, Ϊ������׼����  ::2020-12-26
	strncpy(my_thread->callstack[my_thread->current_depth].name, name, sizeof(my_thread->callstack[0].name)-1);
	my_thread->callstack[(my_thread->current_depth)++].line = line;
	if (my_thread->current_depth > my_thread->maxdepth)			// comment by Clark:: һ���̵߳�ǰ�Ĵ�ӡ���, ���������Ĵ�ӡ�����, ������ˢ�� �ﵽ����maxdepth  ::2020-12-26
		my_thread->maxdepth = my_thread->current_depth;
	if (my_thread->current_depth >= MAX_STACK_DEPTH)			// comment by Clark:: ���̴߳ﵽ������ȳ������������ʱ���򱨴�  ::2020-12-26
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
	// comment by Clark:: ͬһ�������н��ͻ��г�, ���� entry �� exit Ҫ�ɶ�ʹ��   ::2020-12-26
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

// comment by Clark:: ��ӡ�����̵߳����л���  ::2020-12-26
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
