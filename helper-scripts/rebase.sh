#!/bin/sh

###############################################################################
#
# Rebase source tree by applying chages in "old-patched" to "new".
#
# From
#
#           /--> old-patched
#         old --> new
#
# To
#
#           /--> old-patched
#         old --> new --> new-patched
#
# Inputs:
#
#     old = original base tree
#     old-patched = locally edited old base tree
#     new = next-version base tree
#
# Creates:
#
#     new-patched = a derived version of new with (old -> old_patched) 
#                   changes applied
#
#     new-patched.patch = patch file for (new -> new_patched)
#
# Example:
#
#     rebase 3.10 3.10-mypatch 3.12 3.12-mypatch
#
###############################################################################

usage()
{
    echo "Usage: $0 old old-patched new new-patched" >&2
    exit 1
}

canonic_dir()
{
    if ! [ -d "$1" ]; then
        echo "$1" is not a directory
        usage
    fi

    retval=$(readlink -ne "$1")
}

if [ "$#" -ne 4 ]; then
    usage
fi

old_dir="$1"
old_patched_dir="$2"
new_dir="$3"
new_patched_dir="$4"

canonic_dir "$old_dir"
old_dir="$retval"

canonic_dir "$old_patched_dir"
old_patched_dir="$retval"

canonic_dir "$new_dir"
new_dir="$retval"

if [ -e "$old_dir/.git" ]; then
    echo "Error: $old_dir/.git exists"
    exit 1
fi

if [ -e "$old_patched_dir/.git" ]; then
    echo "Error: $old_patched_dir/.git exists"
    exit 1
fi

if [ -e "$new_dir/.git" ]; then
    echo "Error: $new_dir/.git exists"
    exit 1
fi

new_patched_dir=$(readlink -nm "$new_patched_dir")
if [ -e "$new_patched_dir" ]; then
    echo "$new_patched_dir already exists and will be re-created."
    echo "*** Old content will be destroyed ***"
    while true; do
        read -p "OK to proceed? " yn
        case $yn in
            [Yy]*)
	        break
	    ;;
            [Nn]*)
	        echo "Exiting ..."
	        exit 0
	    ;;
            *)
	        echo "Please answer yes or no."
	    ;;
        esac
    done
fi

rm -rf "$new_patched_dir"

rm -rf rebase-temp
mkdir rebase-temp
cd rebase-temp

echo "------------------------------------------------"
echo Creating temporary git in $(readlink -ne .)
echo "........."

echo "------------------------------------------------"
echo Checking in old version "$old_dir"
echo "........."

rsync -a "$old_dir/" .
rm -rf .git
git init
git config merge.renamelimit 999999
git add --all .
git commit -q -m "old"
git checkout -b old_patched

echo "------------------------------------------------"
echo Checking in old_patched version "$old_patched_dir"
echo "........."

rm -rf [a-z]* [A-Z]* [0-9]* .gitignore .mailmap
rsync -a "$old_patched_dir/" .
git add --all .
git commit -q -m "old_patched"

echo "------------------------------------------------"
echo Checking in new version "$new_dir"
echo "........."

git checkout master
rm -rf [a-z]* [A-Z]* [0-9]* .gitignore .mailmap
rsync -a "$new_dir/" .
git add --all .
git commit -q -m "new"

echo "------------------------------------------------"
echo 'Rebasing patch onto "new" version (in master branch)'
echo "........."

git rebase old_patched master

echo "***********************************************************************************"
echo "* "
echo "* If rebase discovered conflicts, edit indicated files to fix the conflict and"
echo '* execute git add" on them.'
echo "* "
echo "* Then execute git rebase --contninue"
echo "* "
echo "* To abort execute git rebase --abort"
echo "* "
echo "* See https://help.github.com/articles/resolving-a-merge-conflict-from-the-command-line"
echo "* "
echo '* Then exit spawned shell back to "rebase.sh"'
echo "* "
echo "***********************************************************************************"

PS1="rebase> " sh

echo ""
echo "Respond: "
echo ""
echo '    c -> resolved conflicts, now execute "git add ." and git rebase --continue'
echo "    C -> no conflicts or already executed git rebase --continue"
echo ""
echo "    a -> execute git rebase --abort"
echo "    A -> already executed git rebase --abort"
echo ""
echo "    x -> just exit for now and complete manually later"
echo ""
while true; do
    read -p "Do you wish to continue / abort? " ca
    case $ca in
        [C]* ) ca="C"; break;;
        [c]* ) ca="c"; break;;
        [A]* ) ca="A"; break;;
        [a]* ) ca="a"; break;;
        [Xx]* ) ca="x"; break;;
        * ) echo "Please answer c/Continue or a/Abort or x/eXit.";;
    esac
done

if [ "$ca" = "x" ]; then
    echo "------------------------------------------------"
    echo 'Exiting ...'
    echo 'Please resolve conflicts and apply "git add" and "git rebase --continue" manually.'
    exit 0
fi

if [ "$ca" = "c" ]; then
    echo "------------------------------------------------"
    echo 'Executing "git rebase --continue"'
    echo "........."
    git add --all .
    git rebase --continue
    ca="C"
fi

if [ "$ca" = "a" ]; then
    echo "------------------------------------------------"
    echo 'Executing "git rebase --abort"'
    echo "........."
    git rebase --abort
    ca="A"
fi

if [ "$ca" = "A" ]; then
    echo "------------------------------------------------"
    echo 'Removing temporary git'
    echo "........."
    cd ..
    rm -rf rebase-temp
    echo "------------------------------------------------"
    exit 0
fi

git checkout master
rm -rf .git
cd ..
mv rebase-temp "$new_patched_dir"

patchname=`basename "$new_patched_dir"`
diff -uprN "$new_dir" "$new_patched_dir" >"$patchname.patch"

echo "------------------------------------------------"
echo ""
echo "Rebasing complete."
echo ""
echo "Rebased tree created in: $new_patched_dir"
echo ""
echo "Rebased patch file created: $patchname.patch"
echo ""

