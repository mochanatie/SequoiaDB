package com.sequoiadb.test.db;

import com.sequoiadb.base.*;
import com.sequoiadb.test.common.Constants;

import static org.junit.Assert.*;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.junit.*;

public class SdbTest {

    private static Sequoiadb sdb;
    private static CollectionSpace cs;
    private static DBCollection cl;
    private static ReplicaGroup rg;
    private static Node node;
    private static DBCursor cursor;

    @BeforeClass
    public static void setConnBeforeClass() throws Exception {
        sdb = new Sequoiadb(Constants.COOR_NODE_CONN, "", "");
    }

    @AfterClass
    public static void DropConnAfterClass() throws Exception {
        sdb.disconnect();
    }

    @Before
    public void setUp() throws Exception {
        if (sdb.isCollectionSpaceExist(Constants.TEST_CS_NAME_1)) {
            sdb.dropCollectionSpace(Constants.TEST_CS_NAME_1);
            cs = sdb.createCollectionSpace(Constants.TEST_CS_NAME_1);
        } else
            cs = sdb.createCollectionSpace(Constants.TEST_CS_NAME_1);
        BSONObject conf = new BasicBSONObject();
        conf.put("ReplSize", 0);
        cl = cs.createCollection(Constants.TEST_CL_NAME_1, conf);
    }

    @After
    public void tearDown() throws Exception {
        sdb.dropCollectionSpace(Constants.TEST_CS_NAME_1);
    }

    @Test
    public void setSessionAttrTest() {
        BSONObject options = new BasicBSONObject();
        options.put("PreferedInstance", 1);
        options.put("PreferedInstanceMode", "ordered");
        options.put("Timeout", -2);
        sdb.setSessionAttr(options);
    }

    @Test
    public void getHostNameAndPortTest() {
        String hostName = sdb.getHost();
        int port = sdb.getPort();
        assertTrue(hostName != null && !hostName.isEmpty());
        assertTrue(port != 0);
        System.out.println(String.format("%s:%d", hostName, port));
    }
    
    @Test
    public void sdbisValid() {
    }
}
