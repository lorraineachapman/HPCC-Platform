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

import lib_stringlib;
import std.str;

d := dataset([{ 'Hello there'}, {'what a nice day'}, {'12$34'}], { string line}) : stored('nofold');

OUTPUT(d(line!='p1'),,PIPE('cmd /C more > out1.csv', csv));
OUTPUT(d(line!='p2'),,PIPE('cmd /C more > out2.csv', csv(terminator('$'),quote('!'))));
OUTPUT(d(line!='p3'),,PIPE('cmd /C more > out1.xml', xml));
OUTPUT(d(line!='p4'),,PIPE('cmd /C more > out2.xml', xml('Zongo', heading('<Zingo>','</Zingo>'))));
OUTPUT(d(line!='p4'),,PIPE('cmd /C more > out3.xml', xml('Zongo', noroot)));
OUTPUT(d(line!='p4'),,PIPE('cmd /C more > out.'+d.line[1]+'.xml', xml('Zongo'), repeat));
