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

// optsort
// Remove adjacent sorts - this time after a filter is moved.

testRecord := RECORD
unsigned4 person_id;
string20  per_surname;
string20  per_forename;
unsigned8 holepos;
    END;

test := DATASET('test',testRecord,FLAT);

a := sort(test, {per_surname});

b := a(per_surname <> 'Hawthorn');

c := sort(test, {per_forename});

output(c,,'out.d00');
