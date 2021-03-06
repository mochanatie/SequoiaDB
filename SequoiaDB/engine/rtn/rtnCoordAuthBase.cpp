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

   Source File Name = rtnCoordAuthBase.cpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          07/12/2012  YW  Initial Draft

   Last Changed =

*******************************************************************************/

#include "rtnCoordAuthBase.hpp"
#include "pmd.hpp"
#include "pmdCB.hpp"
#include "msgMessage.hpp"
#include "pdTrace.hpp"
#include "rtnTrace.hpp"
#include "rtnCoordCommon.hpp"
#include "msgAuth.hpp"

using namespace bson ;

namespace engine
{
   // PD_TRACE_DECLARE_FUNCTION ( SDB_RTNCOAUTHBASE_FORWARD, "rtnCoordAuthBase::forward" )
   INT32 rtnCoordAuthBase::forward( MsgHeader *pMsg,
                                    pmdEDUCB *cb,
                                    INT32 msgType,
                                    BOOLEAN sWhenNoPrimary,
                                    INT64 &contextID )
   {
      INT32 rc = SDB_OK;
      PD_TRACE_ENTRY ( SDB_RTNCOAUTHBASE_FORWARD ) ;
      pmdKRCB *pKrcb                   = pmdGetKRCB();
      CoordCB *pCoordcb                = pKrcb->getCoordCB();
      netMultiRouteAgent *pRouteAgent  = pCoordcb->getRouteAgent();
      pMsg->routeID.value = 0 ;
      pMsg->TID = cb->getTID() ;
      CoordGroupInfoPtr cata ;
      REQUESTID_MAP nodes ;
      REPLY_QUE replyQue ;
      UINT32 times = 0 ;
      UINT32 primaryID = 0 ;

      contextID = -1 ;

      BSONObj authObj ;
      BSONElement user, pass ;
      rc = extractAuthMsg( pMsg, authObj ) ;
      PD_RC_CHECK( rc, PDERROR, "Failed to extrace auth msg, "
                   "rc: %d", rc ) ;
      user = authObj.getField( SDB_AUTH_USER ) ;
      pass = authObj.getField( SDB_AUTH_PASSWD ) ;

      rc = rtnCoordGetCatGroupInfo( cb, FALSE, cata ) ;
      PD_RC_CHECK ( rc, PDWARNING, "Failed to get catalog group info, "
                    "rc = %d", rc  ) ;

   retry:
      nodes.clear() ;
      rc = rtnCoordSendRequestToPrimary( (CHAR*)pMsg,
                                         cata, nodes,
                                         pRouteAgent,
                                         MSG_ROUTE_CAT_SERVICE,
                                         cb ) ;
      if ( SDB_OK != rc )
      {
         if ( sWhenNoPrimary )
         {
            rc = rtnCoordSendRequestToOne( (CHAR*)pMsg, cata,
                                           nodes, pRouteAgent,
                                           MSG_ROUTE_CAT_SERVICE,
                                           cb, TRUE ) ;
            PD_RC_CHECK ( rc, PDERROR, "Can not find a available cata node, "
                          "rc = %d", rc ) ;
         }
         else
         {
            PD_RC_CHECK ( rc, PDERROR, "Can not find the priamry, rc = %d",
                          rc ) ;
         }
      }

      rc = rtnCoordGetReply( cb, nodes, replyQue, msgType ) ;
      PD_RC_CHECK ( rc, PDERROR, "Failed to get reply from catalog for auth, "
                    "rc = %d", rc ) ;

      if ( !replyQue.empty() )
      {
         MsgHeader *res = (MsgHeader*)( replyQue.front() ) ;
         primaryID = MSG_GET_INNER_REPLY_STARTFROM(res) ;
         rc = MSG_GET_INNER_REPLY_RC(res) ;

         if ( rc )
         {
            if ( rtnCoordGroupReplyCheck( cb, rc, _canRetry( times++ ),
                                          res->routeID, cata, NULL,
                                          TRUE, primaryID, TRUE ) )
            {
               rtnClearReplyQue( &replyQue ) ;
               goto retry ;
            }
         }
      }
      else
      {
         PD_RC_CHECK ( SDB_SYS, PDERROR, "Empty reply is received" ) ;
      }

      if ( SDB_OK != rc )
      {
         goto error ;
      }
      else
      {
         cb->setUserInfo( user.valuestrsafe(), pass.valuestrsafe() ) ;
      }

    done:
      rtnClearReplyQue( &replyQue ) ;
      if ( !user.eoo() &&
           ( MSG_AUTH_CRTUSR_REQ == pMsg->opCode ||
             MSG_AUTH_DELUSR_REQ == pMsg->opCode )
         )
      {
         PD_AUDIT_OP( AUDIT_DCL, pMsg->opCode, AUDIT_OBJ_USER,
                      user.valuestrsafe(), rc, "" ) ;
      }
      PD_TRACE_EXITRC ( SDB_RTNCOAUTHBASE_FORWARD, rc ) ;
      return rc ;
   error:
      rtnCoordClearRequest( cb, nodes );
      goto done ;
   }

}

