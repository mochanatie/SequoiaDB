/*******************************************************************************

   Copyright (C) 2011-2014 SequoiaDB Ltd.

   This program is free software: you can redistribute it and/or modify
   it under the term of the GNU Affero General Public License, version 3,
   as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warrenty of
   MARCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program. If not, see <http://www.gnu.org/license/>.

   Source File Name = rtnCoordDCCommands.cpp

   Descriptive Name = Runtime Coord Common

   When/how to use: this program may be used on binary and text-formatted
   versions of runtime component. This file contains code logic for
   common functions for coordinator node.

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          02/11/15    XJH Init
   Last Changed =

*******************************************************************************/

#include "rtnCoordDCCommands.hpp"
#include "msgMessage.hpp"
#include "pmdCB.hpp"

using namespace bson ;

namespace engine
{

   /*
      rtnCoordAlterDC implement
   */
   INT32 rtnCoordAlterDC::execute( MsgHeader *pMsg,
                                   pmdEDUCB *cb,
                                   INT64 &contextID,
                                   rtnContextBuf *buf )
   {
      INT32 rc = SDB_OK ;
      CoordGroupList datagroups ;
      CoordGroupList allgroups ;
      const CHAR *pAction = NULL ;

      contextID                        = -1 ;

      MsgOpQuery *pAttachMsg           = (MsgOpQuery *)pMsg ;
      pAttachMsg->header.opCode        = MSG_CAT_ALTER_IMAGE_REQ ;

      {
         CHAR *pQuery = NULL ;
         rc = msgExtractQuery( (CHAR*)pMsg, NULL, NULL, NULL, NULL,
                               &pQuery, NULL, NULL, NULL ) ;
         PD_RC_CHECK( rc, PDERROR, "Extract command[%s] msg failed, rc: %d",
                      COORD_CMD_ALTER_DC, rc ) ;
         try
         {
            BSONObj objQuery( pQuery ) ;
            BSONElement eleAction = objQuery.getField( FIELD_NAME_ACTION ) ;
            if ( String != eleAction.type() )
            {
               PD_LOG( PDERROR, "The field[%s] is not valid in command[%s]'s "
                       "param[%s]", FIELD_NAME_ACTION, COORD_CMD_ALTER_DC,
                       objQuery.toString().c_str() ) ;
               rc = SDB_INVALIDARG ;
               goto error ;
            }
            pAction = eleAction.valuestr() ;
         }
         catch( std::exception &e )
         {
            rc = SDB_INVALIDARG ;
            PD_LOG( PDERROR, "Parse command[%s]'s param occur exception: %s",
                    COORD_CMD_ALTER_DC, e.what() ) ;
            goto error ;
         }
      }

      rc = executeOnCataGroup( pMsg, cb, &datagroups, NULL, TRUE,
                               NULL, NULL, buf ) ;
      if ( rc )
      {
         PD_LOG( PDERROR, "Failed to execute %s:%s on catalog node, rc: %d",
                 COORD_CMD_ALTER_DC, pAction, rc ) ;
         goto error ;
      }

      rc = rtnCoordGetAllGroupList( cb, allgroups, NULL, FALSE, TRUE ) ;
      if ( rc )
      {
         PD_LOG( PDWARNING, "Failed to update all group list, rc: %d", rc ) ;
         rc = SDB_OK ;
      }

      pAttachMsg->header.opCode        = MSG_BS_QUERY_REQ ;
      if ( 0 == ossStrcasecmp( CMD_VALUE_NAME_ENABLE_READONLY, pAction ) ||
           0 == ossStrcasecmp( CMD_VALUE_NAME_DISABLE_READONLY, pAction ) ||
           0 == ossStrcasecmp( CMD_VALUE_NAME_ACTIVATE, pAction ) ||
           0 == ossStrcasecmp( CMD_VALUE_NAME_DEACTIVATE, pAction ) )
      {
         _executeByNodes( pMsg, cb, allgroups, pAction, buf ) ;
      }
      else
      {
         _executeByGroups( pMsg, cb, allgroups, pAction, buf ) ;
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 rtnCoordAlterDC::_executeByGroups( MsgHeader *pMsg,
                                            pmdEDUCB *cb,
                                            CoordGroupList &groupLst,
                                            const CHAR *pAction,
                                            rtnContextBuf *buf )
   {
      INT32 rc = SDB_OK ;

      rc = executeOnDataGroup( pMsg, cb, groupLst,
                               TRUE, NULL, NULL, NULL, buf ) ;
      if ( rc )
      {
         PD_LOG( PDWARNING, "Failed to execute %s:%s on data groups, "
                 "rc: %d", COORD_CMD_ALTER_DC, pAction, rc ) ;
      }

      return rc ;
   }

   INT32 rtnCoordAlterDC::_executeByNodes( MsgHeader *pMsg,
                                           pmdEDUCB *cb,
                                           CoordGroupList &groupLst,
                                           const CHAR *pAction,
                                           rtnContextBuf *buf )
   {
      INT32 rc = SDB_OK ;
      ROUTE_SET nodes ;
      ROUTE_RC_MAP faileds ;
      rtnCoordCtrlParam ctrlParam ;

      rc = rtnCoordGetGroupNodes( cb, BSONObj(), NODE_SEL_ALL, groupLst,
                                  nodes, NULL ) ;
      if ( rc )
      {
         PD_LOG( PDERROR, "Get group nodes failed, rc: %d", rc ) ;
         goto error ;
      }

      rc = executeOnNodes( pMsg, cb, nodes, faileds, NULL, NULL, NULL ) ;
      if ( rc )
      {
         PD_LOG( PDERROR, "Failed to execute %s:%s on data nodes, "
                 "rc: %d", COORD_CMD_ALTER_DC, pAction, rc ) ;
         goto error ;
      }

   done:
      if ( ( rc || faileds.size() > 0 ) && buf )
      {
         *buf = _rtnContextBuf( rtnBuildErrorObj( rc, cb, &faileds ) ) ;
      }
      return rc ;
   error:
      goto done ;
   }

   /*
      rtnCoordGetDCInfo implement
   */
   INT32 rtnCoordGetDCInfo::_preProcess( rtnQueryOptions &queryOpt,
                                         string &clName,
                                         BSONObj &outSelector )
   {
      clName = CAT_SYSDCBASE_COLLECTION_NAME ;
      queryOpt._query = BSON( FIELD_NAME_TYPE << CAT_BASE_TYPE_GLOBAL_STR ) ;
      return SDB_OK ;
   }

}

