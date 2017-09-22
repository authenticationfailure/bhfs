#!/bin/bash
#
# BHFS - Smoke Test 
# 
# This script tests some basic operations on the filesystem.
# A very basic test, nothing more.
# 

######################## Tests BEGIN #####################
# Add tests here. 
# Make sure all files and dirs start with bhfs_test_
# so that they can be deleted by this script

tests='

ls -l
echo TestContent > bhfs_test_new_file.txt
cat bhfs_test_new_file.txt
stat bhfs_test_new_file.txt
ls -l

ln -s bhfs_test_new_file.txt bhfs_test_new_file_hardlink.txt
ln -s bhfs_test_new_file.txt bhfs_test_new_file_symlink.txt

mv bhfs_test_new_file.txt bhfs_test_renamed_file.txt
rm bhfs_test_renamed_file.txt

dd if=/dev/urandom of=bhfs_test_new_biggish_file.bin bs=1024 count=5


mkdir bhfs_test_new_directory
echo TestContent2 > bhfs_test_new_directory/bhfs_test_nested_file.txt
cd bhfs_test_new_directory
cat bhfs_test_nested_file.txt
cd ..

tar zcvf bhfs_test_archive.tar.gz bhfs_test_*

'
########################## Tests END ######################


test_command() {
	eval "$@" > /dev/null
	ret=$?
	if [ $ret -eq 0 ] ; then
		echo "PASS - command: '$@'"
	else
		echo "FAIL - command: '$@'"
	fi
	return $ret
}

if [ -z "$1" ] ; then
	echo "Usage: $0 directory_to_test" 
	exit 1
fi

test_directory=$1

if [ ! -d "$test_directory" ]; then
	echo "ERROR: The path specified (\"$test_directory\") is not a directory. Exiting!"
	exit 1
fi

cd "$test_directory"

rm -fr bhfs_test_*

while read line; do
	if [ -n "$line" ] ; then 
		test_command $line
		if [ $? -ne 0 ]; then
			echo -e "\n\nTest FAILED :-( "
			rm -fr bhfs_test_*
			exit 1
		fi
	fi
done <<EOF
$tests
EOF

rm -fr bhfs_test_*

echo -e "\nSUCCESS! All tests passed!"

exit 0

