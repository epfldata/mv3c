#! /bin/sh

[ -z "$1" ] && echo "Pass the number of warehouses!" && exit 1
res=$(./test W $1 | grep TPM | cut -d' ' -f3)
echo $res
