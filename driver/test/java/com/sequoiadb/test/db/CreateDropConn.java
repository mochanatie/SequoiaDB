package com.sequoiadb.test.db;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.exception.BaseException;
import com.sequoiadb.test.common.*;

public class CreateDropConn {
	private static Sequoiadb sdb ;
	
	@Before
	public void setUp() throws Exception {
	}

	@After
	public void tearDown() throws Exception {
	}
	@Test
	public void setConnect() throws Exception{
		
			sdb = new Sequoiadb(Constants.COOR_NODE_CONN,"","") ;

			sdb.disconnect();
	}
}

