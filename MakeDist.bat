@echo off

"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip COPYING *.c *.h *.bat *.txt -x todo.txt
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip Amiga/*
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip DOS/* -x DOS/*.exe
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip DOS32/* -x DOS32/*.exe
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip Glk/*
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip Unix/*
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9Src.zip Win/* Win/Classlib/*

