################################################################################
#    Copyright (C) 2011 HPCC Systems.
#
#    All rights reserved. This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as
#    published by the Free Software Foundation, either version 3 of the
#    License, or (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
################################################################################


# Component: roxieclient 

#####################################################
# Description:
# ------------
#    Cmake Input File for roxieclient
#####################################################


project( roxieclient ) 

set (    SRCS 
         roxieclient.cpp 
    )

include_directories ( 
         ./../../system/include 
         ./../../system/jlib 
         ./../../roxie/roxie
    )

ADD_DEFINITIONS( -D_USRDLL -DROXIECLIENT_EXPORTS )

HPCC_ADD_LIBRARY( roxieclient SHARED ${SRCS} )
install ( TARGETS roxieclient DESTINATION ${OSSDIR}/lib )
target_link_libraries ( roxieclient
         jlib
    )


