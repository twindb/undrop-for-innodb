#!/bin/bash

# Adjust your values here
host=localhost
user=root
db=test
table=test
PK=id
newtable=newtest2
# end of user defined values

read -p "Password: " -s password
echo ""
if [ "xxx$password" = "xxx" ]
then
        pass_cmd=""
else
        pass_cmd="-p$password"
fi
mysql -h $host -u $user $pass_cmd -e "show databases" $db
if [ $? -eq 0 ]
then
        echo "Credentials are ok!"
else
        echo "Can't run query SHOW DATABASES"
        echo "Check what's wrong"
        exit
fi
minPK=`mysql -sN -h $host -u $user $pass_cmd -e "SELECT MIN($PK) FROM $table" $db`
maxPK=`mysql -sN -h $host -u $user $pass_cmd -e "SELECT MAX($PK) FROM $table" $db`
a=$minPK
b=$maxPK
let chunk=$b-$a

function insert_range {
q="INSERT IGNORE INTO \`$newtable\` SELECT * FROM \`$table\` WHERE \`$PK\` >= $1 and \`$PK\` <= $2"
mysql -h $host -u $user $pass_cmd -e "$q" $db 2>/dev/null
}
successful_tries=0
echo "Primary key ($PK) range: $minPK .. $maxPK"
while true
do
        insert_range $a $b
        if [ $? -eq 0 ]
        then
                echo "Good, primary key range: $a .. $b"
                let a=$b+1
                let b=$b+$chunk
                let successful_tries=$successful_tries+1
        else
                echo "Bad luck, primary key range: $a .. $b"
                let chunk=$chunk/2
                let b=$a+$chunk
                mysqladmin -h $host -u $user $pass_cmd ping > /dev/null 2>&1
                while [ $? -ne 0 ]
                do
                        mysqladmin -h $host -u $user $pass_cmd ping > /dev/null 2>&1
                done
                successful_tries=0
        fi
        if [ $a -gt $maxPK ]
        then
                exit
        fi
        if [ $chunk -eq 0 ]
        then
                let a=$a+1
                let b=$a+1
                let chunk=1
        fi
        # If things are going well, increase pace
        if [ $successful_tries -gt 5 ]
        then
                let chunk=$chunk*2
        fi
done


