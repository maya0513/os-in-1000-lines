#pragma once
#include "common.h"

// 基本I/O関数
void putchar(char ch);
int getchar(void);
int readfile(const char *filename, char *buf, int len);
int writefile(const char *filename, const char *buf, int len);

// プロセス制御関数
__attribute__((noreturn)) void exit(void);