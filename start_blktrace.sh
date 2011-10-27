#!/bin/sh

if [ -z "$1" ]; then
	echo "Usage: $0 <device to trace>"
	exit -1
fi

if [ -p "blkfifo" ]; then
	echo "Named pipe blkfifo exists."
else
	echo "Create fifo file blkfifo.."
	mkfifo blkfifo || exit -1
fi

exec blktrace -d $1 -a issue -a complete -o - | blkparse -i - | ./blkdata > blkfifo

exit 0

