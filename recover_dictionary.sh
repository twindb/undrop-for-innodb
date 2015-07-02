#!/usr/bin/env bash

set -eu

pages_dir=pages-ibdata1

if ! test -d "$pages_dir"
then
    echo "Please generate $pages_dir with stream-parser."
    echo "Example: ./stream_parser -f /var/lib/mysql/ibdata1"
    exit -1
fi

function get_index_id(){
    case "$1" in
        "SYS_TABLES")
            echo 1; break;;
        "SYS_COLUMNS")
            echo 2; break;;
        "SYS_INDEXES")
            echo 3; break;;
        "SYS_FIELDS")
            echo 4; break;;
        *)
            echo "Unknown table $1"
            exit -1
    esac
}

mkdir -p dumps/default

echo -n "Generating dictionary tables dumps... "
for t in SYS_TABLES SYS_COLUMNS SYS_INDEXES SYS_FIELDS
do
    index_id=`get_index_id $t`
    pages=$pages_dir/FIL_PAGE_INDEX/`printf %016u $index_id`.page
    ./c_parser -4f $pages -t dictionary/$t.sql > dumps/default/$t 2> dumps/default/$t.sql
    ./c_parser -4Df $pages -t dictionary/$t.sql >> dumps/default/$t 2> dumps/default/$t.sql
done
echo "OK"

echo -n "Creating test database ... "
mysql -e "CREATE DATABASE IF NOT EXISTS test"
echo "OK"

echo "Creating dictionary tables in database test:"
for t in SYS_TABLES SYS_COLUMNS SYS_INDEXES SYS_FIELDS
do
    echo -n "$t ... "
    mysql test < dictionary/$t.sql
    echo "OK"
done
echo "All OK"
echo "Loading dictionary tables data:"
for t in SYS_TABLES SYS_COLUMNS SYS_INDEXES SYS_FIELDS
do
    echo -n "$t ... "
    mysql test < dumps/default/$t.sql
    nr=`less dumps/default/$t | grep -v -- '--'| wc -l`
    echo -n "$nr recs "
    echo "OK"
done
echo "All OK"
