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

   Source File Name = rtnUpdate.cpp

   Descriptive Name = Runtime Update

   When/how to use: this program may be used on binary and text-formatted
   versions of Runtime component. This file contains Runtime code for update
   request.

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          09/14/2012  TW  Initial Draft

   Last Changed =

*******************************************************************************/
#include "rtn.hpp"
#include "dmsStorageUnit.hpp"
#include "ossTypes.hpp"
#include "mthModifier.hpp"
#include "rtnIXScanner.hpp"
#include "pmd.hpp"
#include "pmdCB.hpp"
#include "pdTrace.hpp"
#include "rtnTrace.hpp"
#include "dmsScanner.hpp"

using namespace bson ;

namespace engine
{

   // PD_TRACE_DECLARE_FUNCTION ( SDB_RTNUPDATE1, "rtnUpdate" )
   INT32 rtnUpdate ( const CHAR *pCollectionName, const BSONObj &selector,
                     const BSONObj &updator, const BSONObj &hint, INT32 flags,
                     pmdEDUCB *cb, INT64 *pUpdateNum, INT32 *pInsertNum )
   {
      INT32 rc = SDB_OK ;
      PD_TRACE_ENTRY ( SDB_RTNUPDATE1 ) ;
      pmdKRCB *krcb = pmdGetKRCB () ;
      SDB_DMSCB *dmsCB = krcb->getDMSCB () ;
      SDB_DPSCB *dpsCB = krcb->getDPSCB () ;

      if ( dpsCB && cb->isFromLocal() && !dpsCB->isLogLocal() )
      {
         dpsCB = NULL ;
      }

      rc = rtnUpdate ( pCollectionName, selector, updator, hint, flags, cb,
                       dmsCB, dpsCB, 1, pUpdateNum, pInsertNum ) ;

      PD_TRACE_EXITRC ( SDB_RTNUPDATE1, rc ) ;
      return rc ;
   }

   // PD_TRACE_DECLARE_FUNCTION ( SDB_RTNUPDATE2, "rtnUpdate" )
   INT32 rtnUpdate ( const CHAR *pCollectionName, const BSONObj &selector,
                     const BSONObj &updator, const BSONObj &hint, INT32 flags,
                     pmdEDUCB *cb, SDB_DMSCB *dmsCB, SDB_DPSCB *dpsCB,
                     INT16 w, INT64 *pUpdateNum, INT32 *pInsertNum )
   {
      INT32 rc = SDB_OK ;
      PD_TRACE_ENTRY ( SDB_RTNUPDATE2 ) ;

      SDB_ASSERT ( pCollectionName, "collection name can't be NULL" ) ;
      SDB_ASSERT ( cb, "educb can't be NULL" ) ;
      SDB_ASSERT ( dmsCB, "dmsCB can't be NULL" ) ;

      SINT64 numUpdatedRecords         = 0 ;
      INT32  insertNum                 = 0 ;
      dmsStorageUnit *su               = NULL ;
      dmsMBContext   *mbContext        = NULL ;
      dmsStorageUnitID suID            = DMS_INVALID_CS ;
      const CHAR *pCollectionShortName = NULL ;
      rtnAccessPlanManager *apm        = NULL ;
      optAccessPlan *plan              = NULL ;
      BOOLEAN writable                 = FALSE ;
      dmsScanner *pScanner             = NULL ;
      BSONObj emptyObj ;
      mthModifier modifier ;
      vector<INT64> dollarList ;

      if ( updator.isEmpty() )
      {
         PD_LOG ( PDERROR, "modifier can't be empty" ) ;
         rc = SDB_INVALIDARG ;
         goto error ;
      }
      try
      {
         rc = modifier.loadPattern ( updator,
                                     &dollarList ) ;
         PD_RC_CHECK( rc, PDERROR, "Invalid pattern is detected for updator: "
                      "%s", updator.toString().c_str() ) ;
      }
      catch ( std::exception &e )
      {
         PD_LOG ( PDERROR, "Invalid pattern is detected for update: %s: %s",
                  updator.toString().c_str(), e.what() ) ;
         rc = SDB_INVALIDARG ;
         goto error ;
      }

      rc = dmsCB->writable( cb ) ;
      if ( rc )
      {
         PD_LOG ( PDERROR, "Database is not writable, rc = %d", rc ) ;
         goto error;
      }
      writable = TRUE;

      rc = rtnResolveCollectionNameAndLock ( pCollectionName, dmsCB, &su,
                                             &pCollectionShortName, suID ) ;
      if ( rc )
      {
         PD_LOG ( PDERROR, "Failed to resolve collection name %s, rc: %d",
                  pCollectionName, rc ) ;
         goto error ;
      }

      rc = su->data()->getMBContext( &mbContext, pCollectionShortName, -1 ) ;
      PD_RC_CHECK( rc, PDERROR, "Failed to get collection[%s] mb context, "
                   "rc: %d", pCollectionName, rc ) ;

      if ( OSS_BIT_TEST( mbContext->mb()->_attributes,
                         DMS_MB_ATTR_NOIDINDEX ) )
      {
         PD_LOG( PDERROR, "can not update data when autoIndexId is false" ) ;
         rc = SDB_RTN_AUTOINDEXID_IS_FALSE ;
         goto error ;
      }

      apm = su->getAPM() ;
      SDB_ASSERT ( apm, "apm shouldn't be NULL" ) ;

      rc = apm->getPlan ( selector,
                          emptyObj, // orderBy
                          hint, // hint
                          pCollectionShortName,
                          &plan ) ;
      if ( rc )
      {
         PD_LOG ( PDERROR, "Failed to get access plan for %s for update, "
                  "rc: %d", pCollectionName, rc ) ;
         goto error ;
      }

      if ( plan->getScanType() == TBSCAN )
      {
         rc = rtnGetTBScanner( pCollectionShortName, plan->getMatcher(), su,
                               mbContext, cb, &pScanner,
                               DMS_ACCESS_TYPE_UPDATE ) ;
      }
      else if ( plan->getScanType() == IXSCAN )
      {
         rc = rtnGetIXScanner( pCollectionShortName, plan, su, mbContext, cb,
                               &pScanner, DMS_ACCESS_TYPE_UPDATE ) ;
      }
      else
      {
         PD_LOG ( PDERROR, "Invalid return type for scan" ) ;
         rc = SDB_SYS ;
         goto error ;
      }
      PD_RC_CHECK( rc, PDERROR, "Failed to get dms scanner, rc: %d", rc ) ;

      {
         _mthRecordGenerator generator ;
         _mthMatchTreeContext mthContext ;
         dmsRecordID recordID ;
         ossValuePtr recordDataPtr = 0 ;

         mthContext.enableDollarList() ;
         while ( SDB_OK == ( rc = pScanner->advance( recordID, generator,
                                                     cb, &mthContext ) ) )
         {
            mthContext.getDollarList( &dollarList ) ;
            generator.getDataPtr( recordDataPtr ) ;
            rc = su->data()->updateRecord( mbContext, recordID, recordDataPtr,
                                           cb, dpsCB, modifier ) ;
            PD_RC_CHECK( rc, PDERROR, "Update record failed, rc: %d", rc ) ;

            ++numUpdatedRecords ;
            mthContext.clear() ;
            mthContext.enableDollarList() ;
         }

         if ( SDB_DMS_EOC == rc )
         {
            rc = SDB_OK ;
         }
         else if ( rc )
         {
            PD_LOG( PDERROR, "Failed to get next record, rc: %d", rc ) ;
            goto error ;
         }
      }

      if ( ( 0 == numUpdatedRecords ) && ( FLG_UPDATE_UPSERT & flags ) )
      {
         BSONObj source = plan->getMatcher().getEqualityQueryObject() ;
         PD_LOG ( PDDEBUG, "equality query object: %s",
                     source.toString().c_str() ) ;

         BSONObj target ;
         rc = modifier.modify ( source, target ) ;
         if ( rc )
         {
            PD_LOG ( PDERROR, "Failed to generate upsertor record, rc: %d",
                     rc ) ;
            goto error ;
         }
         PD_LOG ( PDDEBUG, "modified equality query object: %s",
                     target.toString().c_str() ) ;

         BSONElement setOnInsert = hint.getField( FIELD_NAME_SET_ON_INSERT ) ;
         if ( !setOnInsert.eoo() )
         {
            rc = rtnUpsertSet( setOnInsert, target ) ;
            PD_RC_CHECK( rc, PDERROR, "failed to set when upsert, rc: %d", rc ) ;
         }

         rc = su->data()->insertRecord( mbContext, target, cb, dpsCB,
                                        TRUE, TRUE ) ;
         if ( rc )
         {
            PD_LOG ( PDERROR, "Failed to insert record %s\ninto collection: %s",
                     target.toString().c_str(), pCollectionShortName ) ;
            goto error ;
         }
         ++insertNum ;
      }

   done :
      if ( pUpdateNum )
      {
         *pUpdateNum = numUpdatedRecords ;
      }
      if ( pInsertNum )
      {
         *pInsertNum = insertNum ;
      }
      if ( pScanner )
      {
         SDB_OSS_DEL pScanner ;
      }
      if ( mbContext )
      {
         su->data()->releaseMBContext( mbContext ) ;
      }
      if ( plan )
      {
         plan->release() ;
      }
      if ( DMS_INVALID_CS != suID )
      {
         dmsCB->suUnlock ( suID ) ;
      }
      if ( writable )
      {
         dmsCB->writeDown( cb ) ;
      }
      if ( cb )
      {
         if ( SDB_OK == rc && dpsCB )
         {
            rc = dpsCB->completeOpr( cb, w ) ;
         }
      }
      PD_TRACE_EXITRC ( SDB_RTNUPDATE2, rc ) ;
      return rc ;
   error :
      goto done ;
   }

   INT32 rtnUpsertSet( const BSONElement& setOnInsert, BSONObj& target )
   {
      INT32 rc = SDB_OK ;

      try
      {
         BSONObj newTarget ;
         BSONObj setObj ;
         BSONObjBuilder builder ;
         builder.appendAs( setOnInsert, "$set" ) ;
         setObj = builder.obj() ;

         mthModifier setModifier ;
         rc = setModifier.loadPattern( setObj ) ;
         PD_RC_CHECK( rc, PDERROR, "Invalid pattern is detected: { %s }, rc: %d",
                      setOnInsert.toString().c_str(), rc ) ;
         rc = setModifier.modify( target, newTarget ) ;
         PD_RC_CHECK( rc, PDERROR, "failed to generate upsertor "
                      "record(rc=%d) by " FIELD_NAME_SET_ON_INSERT, rc ) ;

         target = newTarget ;
      }
      catch ( std::exception &e )
      {
         PD_LOG ( PDERROR, "failed to generate upsertor on { %s }, %s",
                  setOnInsert.toString().c_str(), e.what() ) ;
         rc = SDB_INVALIDARG ;
         goto error ;
      }

   done:
      return rc ;
   error :
      goto done ;
   }

}

