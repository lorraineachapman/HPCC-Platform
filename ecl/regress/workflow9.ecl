/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */


namesRecord :=
            RECORD
string20        surname;
string10        forename;
integer2        age := 25;
            END;

namesTable1 := dataset([
        {'Hawthorn','Gavin',31},
        {'X','Z'}], namesRecord);

o1 := output(namesTable1,,'~names',overwrite);


namesTable2 := dataset([
        {'Hawthorn','Gavin',31},
        {'Hawthorn','Mia',31},
        {'Hawthorn','Robert',0},
        {'X','Z'}], namesRecord);

o2 := output(namesTable2,,'~names',overwrite);



namesTable := dataset('~names',namesRecord,FLAT);

p := namesTable(age <> 0) : persist('p1');



boolean use1 := true : stored('use1');

sequential(
    if (use1, o1, o2),
    output(count(p))
    );


/*
Expected:

1:  stored
2:  persist()
3:  normal [1]          if...
4:  normal [2]          output(count)
5:  sequential [3,4]

*/
