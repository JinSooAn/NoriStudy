#include "CRv.hpp"

//-----------------------------------------------------------------------------------------------------------------------------------------------
//  RV Sender
//-----------------------------------------------------------------------------------------------------------------------------------------------
CRvSender::CRvSender( CLogFile *pLog, TibrvNetTransport *pTransport )
{
    mainLog = pLog;

    m_pTransport = pTransport;

    //Debug("CRvSender()");
}
CRvSender::~CRvSender( )
{
    //Debug("~CRvSender()");
}

//int CRvSender::SendRvMsg( CString cszToSubject, CString cszSendMsg, RV_SEND_TYPE ervSendType, CString cszSendMsgBinary )
int CRvSender::SendRvMsg( CString cszToSubject, CString cszSendMsg, int ervSendType, CString cszSendMsgBinary, CString cszSendMsgPPBODY, CString cszSendMsgDMS )
{
    TibrvStatus status;
    TibrvMsg msg;

    // Set subject into message
    status = msg.setSendSubject( cszToSubject.String() );
    if (status != TIBRV_OK)
    {
        //Debug("(CTRL)  could not set subject %s into message, status=%d, text=%s", cszToSubject.String(), (int) status, status.getText() );
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: could not set subject %s into message, status=%d, text=%s", cszToSubject.String(), (int) status, status.getText() );
        return FALSE;
    }

    if ( cszSendMsg.Length() > 0)
    {
        ervSendType |= MESSAGE;         // Text ���ڿ� ���� ������ MESSAGE �߰� �Ѵ�.
    }

    // Make Binary Opaque Item 
    //if ( ervSendType == MESSAGE_BINARY )
    if ( ervSendType & MESSAGE_BINARY )  
    {
        status = msg.updateOpaque (FIELD_NAME_BINARY, cszSendMsgBinary.String(), cszSendMsgBinary.Length() );
        if (status != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: error sending message binary set subject %s (binary-length) %d into message, status=%d, text=%s",
                                                    cszToSubject.String(), cszSendMsgBinary.Length(), (int) status, status.getText() );
            return FALSE;
        }

        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: CRvSender_SendRvMsg::: Length=%d", cszSendMsgBinary.Length() );
    }

    if ( ervSendType & MESSAGE_PPBODY )  
    {
        status = msg.updateOpaque (FIELD_NAME_PPBODY, cszSendMsgPPBODY.String(), cszSendMsgPPBODY.Length() );
        if (status != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: error sending message PPBODY set subject %s (PPBODY-length) %d into message, status=%d, text=%s",
                            cszToSubject.String(), cszSendMsgPPBODY.Length(), (int) status, status.getText() );
            return FALSE;
        }
    }

    if ( ervSendType & MESSAGE_DMS )  
    {
        status = msg.updateOpaque (FIELD_NAME_DMS, cszSendMsgDMS.String(), cszSendMsgDMS.Length() );
        if (status != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: error sending message DMS set subject %s (DMS-length) %d into message, status=%d, text=%s",
                            cszToSubject.String(), cszSendMsgDMS.Length(), (int) status, status.getText() );
            return FALSE;
        }
    }

    if ( ervSendType & MESSAGE )  
    {
        // Update the string in our message
        status = msg.updateString( FIELD_NAME, cszSendMsg.String() );
        if (status != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: Error sending message string, status=%d, text=%s", (int) status, status.getText() );
            return FALSE;
        }
    }

    status = m_pTransport->send(msg);

    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvSender::SendRvMsg) :: Error sending message string, status=%d, text=%s", (int) status, status.getText() );
        return FALSE;
    }
    else
        return TRUE;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------
//  RV Listener
//-----------------------------------------------------------------------------------------------------------------------------------------------
CRvListener::CRvListener( CLogFile *pLog, RV_BLOCK BlockMode, TibrvNetTransport *pTransport )
{
    mainLog     = pLog;
    m_BlockMode = BlockMode;
    m_pTransport = pTransport;

    m_listener  = (TibrvListener *) NULL;

    Clear();

    //Debug("CRvListener()");
}
CRvListener::~CRvListener()
{
    //Debug("~CRvListener()");

    if ( m_listener ) delete m_listener;
}

void CRvListener::Clear()
{
    m_rvReplySubject  = "";
    m_rvmessage       = "";
    m_rvmessageBinary = "";
    m_rvmessagePPBODY = "";
    m_rvmessageDMS    = "";
    m_rvReadType      = 0;
    m_rvRead          = FALSE;
}

void CRvListener::onMsg( TibrvListener* listener, TibrvMsg& msg )
{
    const char*     msgString    = NULL;
    const char*     sendSubject  = NULL;
    const char*     replySubject = NULL;

    tibrv_u32       numFields; 
    TibrvMsgField   msgField;
    TibrvStatus     rvStatus;

    msg.getSendSubject ( sendSubject );
    msg.getReplySubject ( replySubject );

    m_rvReplySubject = (char *)replySubject;

    rvStatus = msg.getNumFields ( numFields );
    if (rvStatus != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: received   getNumFields() Error [%s] subject [%s] reply subject [%s] ", 
                                                 rvStatus.getText(), sendSubject, replySubject );
        return;
    }

    // Default Message Type Set
    m_rvReadType = MESSAGE;

    for ( unsigned int nFieldLoop = 0; nFieldLoop < numFields;  nFieldLoop++)
    {
        rvStatus = msg.getFieldByIndex ( msgField, nFieldLoop );
        if (rvStatus != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: received   getFieldByIndex() Error [%s] subject [%s] reply subject [%s] ", 
                                                     rvStatus.getText(), sendSubject, replySubject );
            return;
        }

        // Get Field Name
        CString cszFieldName = (char *) msgField.getName();

        if ( cszFieldName == FIELD_NAME )
        {
            m_rvmessage   = (char *) msgField.getData().str;
            m_rvReadType |= MESSAGE;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: ..... RV Read MESSAGE Data. Length=%d",m_rvmessage.Length() );
        }
        else if ( cszFieldName == FIELD_NAME_BINARY )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessageBinary = cszTempBinaryData;
            m_rvReadType     |= MESSAGE_BINARY;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: ..... RV Read Binary Data. Length=%d",(unsigned int) msgField.getSize() );
        }
        else if ( cszFieldName == FIELD_NAME_PPBODY )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessagePPBODY = cszTempBinaryData;
            m_rvReadType     |= MESSAGE_PPBODY;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: ..... RV Read PPBODY Data. Length=%d",(unsigned int) msgField.getSize()  );
        }
        else if ( cszFieldName == FIELD_NAME_DMS )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessageDMS = cszTempBinaryData;
            m_rvReadType  |= MESSAGE_DMS;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: ..... RV Read DMS Data. Length=%d",(unsigned int) msgField.getSize()  );
        }
        /*else 
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: received   Not Defined RV FIELD  Name [%s] subject [%s] reply subject [%s] ", 
                                        cszFieldName.String(), sendSubject, replySubject );
            return;
        }*/
        // MESO ���� AF_INFO, AF_CNT Field �� ����ϰ� �־�, DATA �κ��� ó������ ���ϴ� ������ �־� return ���� �ʵ��� ��
        /*else 
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::onMsg) :: received   Not Defined RV FIELD  Name [%s] subject [%s] reply subject [%s] ", 
                                                        cszFieldName.String(), sendSubject, replySubject );

            return;   
        }*/
    }
    m_rvRead = TRUE;
}

int CRvListener::Create_Listner( CString rv_subject )
{
    TibrvStatus status;

    // Create listeners for specified subjects.
    // In this test program we never delete listener objects.

    //Debug("RV Listen String : '%s'", rv_subject.String() );

    m_listener = new TibrvListener();

    status = m_listener->create ( Tibrv::defaultQueue(), this, m_pTransport, rv_subject.String() );
    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::Create_Listner) :: could not create listener on %s, status=%d, text=%s", rv_subject.String(), (int)status, status.getText());
        return FALSE;
    }
    else
        return TRUE;
}

//int CRvListener::GetRvMessage ( CString &cszRvMsg, RV_SEND_TYPE &eRvReadType, CString &cszRvMsgBin )
int CRvListener::GetRvMessage ( CString &cszRvMsg, int &eRvReadType, CString &cszRvMsgBin, int nTimeout /*= 0*/ )
{
    TibrvStatus status;
    // dispatch Tibrv events
    Clear();

    if ( m_BlockMode == RV_MSG_NON_BLOCK )
    {
        if( nTimeout == 0 )
        {
            //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->poll() 1: nTimeout=%d", nTimeout);
            status = Tibrv::defaultQueue()->poll();                         //Non Blocking
        }else
        {
            time_t tm_st;
            time_t tm_nd;
            double d_diff;

            time(&tm_st);

            while( 1 )
            {
                //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->poll() 2: nTimeout=%d", nTimeout);
                status = Tibrv::defaultQueue()->poll();                         //Non Blocking

                if ( status != TIBRV_OK && status != TIBRV_TIMEOUT )   // Error �߻��� ��� Break ó��
                {
                    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
                    break;
                }

                if ( m_rvRead == TRUE )   // Rv Message ������ ��� Break ó��
                {
                    break;
                }

                time(&tm_nd);
                d_diff = difftime(tm_nd, tm_st);

                if ( d_diff > nTimeout )   // Timer �ð� Over�� Error ó��
                {
                    cszRvMsg    = "RV ERROR :: TIMEOUT"; // Error �޽��� ����
                    return FALSE;
                }

                usleep(100000);  //0.1 ��
            }
        }
    }else
    {
        if( nTimeout == 0 )
        {
            //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->dispatch()");
            status = Tibrv::defaultQueue()->dispatch();                 //Blocking
        }else
        {
            //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->timedDispatch( %d )", (int) nTimeout);
            status = Tibrv::defaultQueue()->timedDispatch( nTimeout );  //Timed Blocking
        }

        if ( status == TIBRV_TIMEOUT )
        {
            cszRvMsg    = "RV ERROR :: TIMEOUT"; // Error �޽��� ����
        }
    }

    if ( m_rvRead == FALSE && status != TIBRV_OK )
    {
        if ( status != TIBRV_TIMEOUT )
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
        }

        return FALSE;
    }

    if ( status != TIBRV_OK )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: .. (!TIBRV_OK) : status=%d, text=%s", (int) status, status.getText());
    }

    // P R O C   R E C E I V E   R E N D E Z V O U S   M E S S A G E
    if ( m_rvRead == TRUE  && m_rvmessage.Length() > 0 )
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: RECV_RV: '%s'", m_rvmessage.String() );
        //Debug( "(RECV_RV) ::: '%s'", m_rvmessage.String());

        cszRvMsg    = m_rvmessage;
        eRvReadType = m_rvReadType;
        cszRvMsgBin = (eRvReadType & MESSAGE_BINARY)? m_rvmessageBinary: NULL;

        return TRUE;
    }
    else
        return FALSE;
}

int CRvListener::GetRvMessage ( CString &cszRvMsg, int &eRvReadType, int nTimeout /*= 0*/ )
{
    TibrvStatus status;
    // dispatch Tibrv events
    Clear();

    if ( m_BlockMode == RV_MSG_NON_BLOCK )
    {
        if( nTimeout == 0 )
        {
            //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->poll() 1: nTimeout=%d", nTimeout);
            status = Tibrv::defaultQueue()->poll();                         //Non Blocking
        }else
        {
            time_t tm_st;
            time_t tm_nd;
            double d_diff;

            time(&tm_st);

            while( 1 )
            {
                //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->poll() 2: nTimeout=%d", nTimeout);
                status = Tibrv::defaultQueue()->poll();                         //Non Blocking

                if ( status != TIBRV_OK && status != TIBRV_TIMEOUT )   // Error �߻��� ��� Break ó��
                {
                    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
                    break;
                }

                if ( m_rvRead == TRUE )   // Rv Message ������ ��� Break ó��
                {
                    break;
                }

                time(&tm_nd);
                d_diff = difftime(tm_nd, tm_st);

                if ( d_diff > nTimeout )   // Timer �ð� Over�� Error ó��
                {
                    cszRvMsg    = "RV ERROR :: TIMEOUT"; // Error �޽��� ����
                    return FALSE;
                }

                usleep(300000);  //0.3 ��
            }
        }
    }else
    {
        if( nTimeout == 0 )
        {
            //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->dispatch()");
            status = Tibrv::defaultQueue()->dispatch();                 //Blocking
        }else
        {
            //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB3, "(CRvListener::GetRvMessage) :: Tibrv::defaultQueue()->timedDispatch( %d )", (int) nTimeout);
            status = Tibrv::defaultQueue()->timedDispatch( nTimeout );  //Timed Blocking
        }

        if ( status == TIBRV_TIMEOUT )
        {
            cszRvMsg    = "RV ERROR :: TIMEOUT"; // Error �޽��� ����
        }
    }

//Debug("GetRvMessage ::: dispatch()....end!!!");

    if ( m_rvRead == FALSE && status != TIBRV_OK )
    {
        if ( status != TIBRV_TIMEOUT )
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
        }

        return FALSE;
    }

    if ( status != TIBRV_OK )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: .. (!TIBRV_OK) : status=%d, text=%s", (int) status, status.getText());
    }

    // P R O C   R E C E I V E   R E N D E Z V O U S   M E S S A G E
    if ( m_rvRead == TRUE )  
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvListener::GetRvMessage) :: RECV_RV : '%s'", m_rvmessage.String() );
        //Debug( "(RECV_RV) ::: '%s'", m_rvmessage.String());

        eRvReadType = m_rvReadType;

        if ( eRvReadType & MESSAGE )  cszRvMsg    = m_rvmessage; // �޼����� type�� ����
        
        return TRUE;
    }
    else
        return FALSE;
}
// GetRvMessage �� ȣ���� ��, eRvReadType �� PPBODY, DMS �� ��� ������ �Ʒ� �� �Լ��� �̿��� Bin �� ���� �Ѵ�
CString CRvListener::GetBinBinary (  )
{
    return m_rvmessageBinary;
}
CString CRvListener::GetBinPPBODY (  )
{
    return m_rvmessagePPBODY;
}
CString CRvListener::GetBinDMS (  )
{
    return m_rvmessageDMS;
}
CString CRvListener::GetReplySubject (  )
{
    return m_rvReplySubject;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------
//  RV CMQueue Listener
//  2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
//-----------------------------------------------------------------------------------------------------------------------------------------------
CRvCMQListener::CRvCMQListener( CLogFile *pLog, RV_BLOCK BlockMode, TibrvNetTransport *pTransport )
{
    mainLog         = pLog;
    m_BlockMode     = BlockMode;
    m_pTransport    = pTransport;

    m_pCMListener   = (TibrvCmListener *) NULL;
    m_pQueue        = (TibrvQueue *) NULL;
    m_pCMQTransport = (TibrvCmQueueTransport *) NULL;

    Clear();

    //Debug("CRvCMQListener()");
}

CRvCMQListener::~CRvCMQListener()
{
    //Debug("~CRvCMQListener()");

    if ( m_pCMListener )   delete m_pCMListener;
    if ( m_pQueue )        delete m_pQueue;
    if ( m_pCMQTransport ) delete m_pCMQTransport;
}

void CRvCMQListener::Clear()
{
    m_rvReplySubject  = "";
    m_rvmessage       = "";
    m_rvmessageBinary = "";
    m_rvmessagePPBODY = "";
    m_rvmessageDMS    = "";
    m_rvReadType      = 0;
    m_rvRead          = FALSE;
}

void CRvCMQListener::onCmMsg( TibrvCmListener* listener, TibrvMsg& msg )
{
    const char*     msgString    = NULL;
    const char*     sendSubject  = NULL;
    const char*     replySubject = NULL;

    tibrv_u32       numFields; 
    TibrvMsgField   msgField;
    TibrvStatus     rvStatus;

    msg.getSendSubject ( sendSubject );
    msg.getReplySubject ( replySubject );

    m_rvReplySubject = (char *)replySubject;

    rvStatus = msg.getNumFields ( numFields );
    if (rvStatus != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: received   getNumFields() Error [%s] subject [%s] reply subject [%s] ", 
                                                rvStatus.getText(), sendSubject, replySubject );
        return;
    }

    // Default Message Type Set
    m_rvReadType = MESSAGE;

    for ( unsigned int nFieldLoop = 0; nFieldLoop < numFields;  nFieldLoop++)
    {
        rvStatus = msg.getFieldByIndex ( msgField, nFieldLoop );
        if (rvStatus != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: received   getFieldByIndex() Error [%s] subject [%s] reply subject [%s] ", 
                                                    rvStatus.getText(), sendSubject, replySubject );
            return;
        }

        // Get Field Name
        CString cszFieldName = (char *) msgField.getName();

        if ( cszFieldName == FIELD_NAME )
        {
            m_rvmessage   = (char *) msgField.getData().str;
            m_rvReadType |= MESSAGE;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: ..... RV Read MESSAGE Data. Length=%d",m_rvmessage.Length() );
        }
        else if ( cszFieldName == FIELD_NAME_BINARY )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessageBinary = cszTempBinaryData;
            m_rvReadType     |= MESSAGE_BINARY;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: ..... RV Read Binary Data. Length=%d",(unsigned int) msgField.getSize() );
        }
        else if ( cszFieldName == FIELD_NAME_PPBODY )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessagePPBODY = cszTempBinaryData;
            m_rvReadType     |= MESSAGE_PPBODY;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: ..... RV Read PPBODY Data. Length=%d",(unsigned int) msgField.getSize()  );
        }
        else if ( cszFieldName == FIELD_NAME_DMS )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessageDMS = cszTempBinaryData;
            m_rvReadType  |= MESSAGE_DMS;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: ..... RV Read DMS Data. Length=%d",(unsigned int) msgField.getSize()  );
        }
        /*else 
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: received   Not Defined RV FIELD  Name [%s] subject [%s] reply subject [%s] ", 
                                                    cszFieldName.String(), sendSubject, replySubject );
            return;
        }*/
        // MESO ���� AF_INFO, AF_CNT Field �� ����ϰ� �־�, DATA �κ��� ó������ ���ϴ� ������ �־� return ���� �ʵ��� ��
        /*else 
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::onCmMsg) :: received   Not Defined RV FIELD  Name [%s] subject [%s] reply subject [%s] ", 
                                                    cszFieldName.String(), sendSubject, replySubject );

            return;   
        }*/
    }

    m_rvRead = TRUE;
}

int CRvCMQListener::Create_Listner( CString rv_subject, CString cszDQIdentifyID )
{
    TibrvStatus status;

    // Create listeners for specified subjects.
    // In this test program we never delete listener objects.

    //Debug("RV Listen String : '%s'", rv_subject.String() );

    // Create queue
    m_pQueue = new TibrvQueue();
    m_pQueue->create();

    m_pCMQTransport = new TibrvCmQueueTransport();

    status = m_pCMQTransport->create ( m_pTransport, cszDQIdentifyID.String() );
    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::Create_Listner) :: could not create TibrvCmQueueTransport on %s, status=%d, text=%s", cszDQIdentifyID.String(), (int)status, status.getText());
        return FALSE;
    }

    m_pCMListener = new TibrvCmListener();

    status = m_pCMListener->create ( m_pQueue, this, m_pCMQTransport, rv_subject.String() );

    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::Create_Listner) :: could not create m_pCMListener on %s, status=%d, text=%s", rv_subject.String(), (int)status, status.getText());
        return FALSE;
    }
    else
        return TRUE;
}

int CRvCMQListener::GetRvMessage ( CString &cszRvMsg, int &eRvReadType, CString &cszRvMsgBin, int nTimeout /*= 0*/ )
{
    TibrvStatus status;

    // dispatch Tibrv events
    Clear();

    if ( m_BlockMode == RV_MSG_NON_BLOCK )
    {
        status = m_pQueue->poll();                         //Non Blocking
    }else
    {
        if( nTimeout == 0 )
        {
            status = m_pQueue->dispatch();                 //Blocking
        }else
        {
            status = m_pQueue->timedDispatch( nTimeout );  //Timed Blocking
        }
    }

    if ( m_rvRead == FALSE && status != TIBRV_OK )
    {
        if ( status != TIBRV_TIMEOUT )
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
        }

        return FALSE;
    }

    if ( status != TIBRV_OK )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::GetRvMessage) :: .. (!TIBRV_OK) : status=%d, text=%s", (int) status, status.getText());
    }

    // P R O C   R E C E I V E   R E N D E Z V O U S   M E S S A G E
    if ( m_rvRead == TRUE  && m_rvmessage.Length() > 0 )
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::GetRvMessage) :: RECV_RV : %s", m_rvmessage.String() );
        //Debug( "(RECV_RV) ::: '%s'", m_rvmessage.String());

        cszRvMsg    = m_rvmessage;
        eRvReadType = m_rvReadType;
        cszRvMsgBin = (eRvReadType & MESSAGE_BINARY)? m_rvmessageBinary: NULL;

        return TRUE;
    }
    else
        return FALSE;
}

int CRvCMQListener::GetRvMessage ( CString &cszRvMsg, int &eRvReadType, int nTimeout /*= 0*/ )
{
    TibrvStatus status;

    // dispatch Tibrv events
    Clear();

    if ( m_BlockMode == RV_MSG_NON_BLOCK )
    {
        status = m_pQueue->poll();                         //Non Blocking
    }else
    {
        if( nTimeout == 0 )
        {
            status = m_pQueue->dispatch();                 //Blocking
        }else
        {
            status = m_pQueue->timedDispatch( nTimeout );  //Timed Blocking
        }
    }

//Debug("GetRvMessage ::: dispatch()....end!!!");

    if ( m_rvRead == FALSE && status != TIBRV_OK )
    {
        if ( status != TIBRV_TIMEOUT )
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
        }

        return FALSE;
    }

    if ( status != TIBRV_OK )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::GetRvMessage) :: .. (!TIBRV_OK) : status=%d, text=%s", (int) status, status.getText());
    }

    // P R O C   R E C E I V E   R E N D E Z V O U S   M E S S A G E
    if ( m_rvRead == TRUE )  
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvCMQListener::GetRvMessage) :: RECV_RV : %s", m_rvmessage.String() );
        //Debug( "(RECV_RV) ::: '%s'", m_rvmessage.String());

        eRvReadType = m_rvReadType;

        if ( eRvReadType & MESSAGE )  cszRvMsg    = m_rvmessage; // �޼����� type�� ����
        
        return TRUE;
    }
    else
        return FALSE;
}

// GetRvMessage �� ȣ���� ��, eRvReadType �� PPBODY, DMS �� ��� ������ �Ʒ� �� �Լ��� �̿��� Bin �� ���� �Ѵ�
CString CRvCMQListener::GetBinBinary (  )
{
    return m_rvmessageBinary;
}
CString CRvCMQListener::GetBinPPBODY (  )
{
    return m_rvmessagePPBODY;
}
CString CRvCMQListener::GetBinDMS (  )
{
    return m_rvmessageDMS;
}
CString CRvCMQListener::GetReplySubject (  )
{
    return m_rvReplySubject;
}


//-----------------------------------------------------------------------------------------------------------------------------------------------
//  RV FT Listener
//-----------------------------------------------------------------------------------------------------------------------------------------------
CRvFTListener::CRvFTListener( CLogFile *pLog, RV_BLOCK BlockMode, TibrvNetTransport *pTransport )
{
    mainLog         = pLog;
    m_BlockMode     = BlockMode;
    m_pTransport    = pTransport;

    m_listener      = (TibrvListener *) NULL;
    active          = false;
    Clear();

    //Debug("CRvFTListener()");
}

CRvFTListener::~CRvFTListener()
{
    //Debug("~CRvFTListener()");

    if ( m_listener ) delete m_listener;
}

void CRvFTListener::Clear()
{
    m_rvReplySubject  = "";
    m_rvmessage       = "";
    m_rvmessageBinary = "";
    m_rvmessagePPBODY = "";
    m_rvmessageDMS    = "";
    m_rvReadType      = 0;
    m_rvRead          = FALSE;
}

void CRvFTListener::onMsg( TibrvListener* listener, TibrvMsg& msg )
{
    const char*     msgString    = NULL;
    const char*     sendSubject  = NULL;
    const char*     replySubject = NULL;

    tibrv_u32       numFields; 
    TibrvMsgField   msgField;
    TibrvStatus     rvStatus;

    //Debug("CRvFTListener::onMsg()");

    msg.getSendSubject ( sendSubject );
    msg.getReplySubject ( replySubject );

    m_rvReplySubject = (char *)replySubject;

    rvStatus = msg.getNumFields ( numFields );
    if (rvStatus != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: received   getNumFields() Error [%s] subject [%s] reply subject [%s] ", 
                                                 rvStatus.getText(), sendSubject, replySubject );
        return;
    }

    // Default Message Type Set
    m_rvReadType = MESSAGE;

    for ( unsigned int nFieldLoop = 0; nFieldLoop < numFields;  nFieldLoop++)
    {
        rvStatus = msg.getFieldByIndex ( msgField, nFieldLoop );
        if (rvStatus != TIBRV_OK)
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: received   getFieldByIndex() Error [%s] subject [%s] reply subject [%s] ", 
                                                     rvStatus.getText(), sendSubject, replySubject );
            return;
        }

        // Get Field Name
        CString cszFieldName = (char *) msgField.getName();

        if ( cszFieldName == FIELD_NAME )
        {
            m_rvmessage   = (char *) msgField.getData().str;
            m_rvReadType |= MESSAGE;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: ..... RV Read MESSAGE Data. Length=%d",m_rvmessage.Length() );
        }
        else if ( cszFieldName == FIELD_NAME_BINARY )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessageBinary = cszTempBinaryData;
            m_rvReadType     |= MESSAGE_BINARY;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: ..... RV Read Binary Data. Length=%d",(unsigned int) msgField.getSize() );
        }
        else if ( cszFieldName == FIELD_NAME_PPBODY )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessagePPBODY = cszTempBinaryData;
            m_rvReadType     |= MESSAGE_PPBODY;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: ..... RV Read PPBODY Data. Length=%d",(unsigned int) msgField.getSize()  );
        }
        else if ( cszFieldName == FIELD_NAME_DMS )
        {
            CString cszTempBinaryData ( (char *) (unsigned char *) msgField.getData().buf, (unsigned int) msgField.getSize() );

            m_rvmessageDMS = cszTempBinaryData;
            m_rvReadType  |= MESSAGE_DMS;
//          if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: ..... RV Read DMS Data. Length=%d",(unsigned int) msgField.getSize()  );
        }
        /*else 
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: received   Not Defined RV FIELD  Name [%s] subject [%s] reply subject [%s] ", 
                                        cszFieldName.String(), sendSubject, replySubject );
            return;
        }*/
        // MESO ���� AF_INFO, AF_CNT Field �� ����ϰ� �־�, DATA �κ��� ó������ ���ϴ� ������ �־� return ���� �ʵ��� ��
        /*else 
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onMsg) :: received   Not Defined RV FIELD  Name [%s] subject [%s] reply subject [%s] ", 
                                                        cszFieldName.String(), sendSubject, replySubject );

            return;   
        }*/
    }
    m_rvRead = TRUE;
}

void CRvFTListener::onFtAction( TibrvFtMember * ftMember,
                              const char*     groupName,
                              tibrvftAction   action )
{
    //Debug("CRvFTListener::onFtAction() : Start");
    TibrvStatus status;

    unsigned short lnWeight;
    status = ftMember->getWeight( lnWeight );
    if (action == TIBRVFT_PREPARE_TO_ACTIVATE)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onFtAction) :: PREPARE_TO_ACTIVATE invoked... GroupName=[%s], Weight=[%ld]", groupName, lnWeight );
    }
    else if (action == TIBRVFT_ACTIVATE)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onFtAction) :: ACTIVATE invoked... GroupName=[%s], Weight=[%ld]", groupName, lnWeight );
        Create_Listner();
        active = true;
    }
    else if (action == TIBRVFT_DEACTIVATE) 
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::onFtAction) :: DEACTIVATE invoked... GroupName=[%s], Weight=[%ld]", groupName, lnWeight );
        Destroy_Listner();
        active = false;
    }
}

int CRvFTListener::Create_Listner( )
{
    TibrvStatus status;

    // Create listeners for specified subjects.
    // In this test program we never delete listener objects.
    m_listener = new TibrvListener();

    status = m_listener->create ( Tibrv::defaultQueue(), this, m_pTransport, m_rvMySubject.String() );
    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::Create_Listner) :: could not create listener on %s, status=%d, text=%s", m_rvMySubject.String(), (int)status, status.getText());
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


int CRvFTListener::Destroy_Listner( )
{
    TibrvStatus status;

    status = m_listener->destroy();
    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::Destroy_Listner) :: could not destroy listener on %s, status=%d, text=%s", m_rvMySubject.String(), (int)status, status.getText());
        return FALSE;
    }
    else
    {
        if ( m_listener ) delete m_listener;
        m_listener  = (TibrvListener *) NULL;
        return TRUE;
    }
}

int CRvFTListener::Create_FTMember( TibrvNetTransport *pFTTransport, CString cszGroupName, long nWeight, long nActiveGoalNum, 
                             double fHBInterval, double fPrepareInterval, double fActivateInterval )
{
    TibrvStatus status;

    // Create listeners for specified subjects.
    // In this test program we never delete listener objects.

    m_pFTMember = new TibrvFtMember();

    status = m_pFTMember->create ( Tibrv::defaultQueue(), this, pFTTransport, cszGroupName.String(), nWeight, nActiveGoalNum, fHBInterval, fPrepareInterval, fActivateInterval, null );

    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::Create_FTMember) :: could not create FtMember on %s, status=%d, text=%s", m_rvMySubject.String(), (int)status, status.getText());
        return FALSE;
    }
    else
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::Create_FTMember) :: Create FtMember on %s, status=%d, text=%s", m_rvMySubject.String(), (int)status, status.getText());
        return TRUE;
    }
}

void CRvFTListener::Set_MySubject( CString cszRvSubject )
{
    m_rvMySubject   = cszRvSubject;
}

//int CRvFTListener::GetRvMessage ( CString &cszRvMsg, RV_SEND_TYPE &eRvReadType, CString &cszRvMsgBin )
int CRvFTListener::GetRvMessage ( CString &cszRvMsg, int &eRvReadType, CString &cszRvMsgBin, int nTimeout /*= 0*/ )
{
    TibrvStatus status;

    // dispatch Tibrv events
    Clear();

    if ( m_BlockMode == RV_MSG_NON_BLOCK )
    {
        status = Tibrv::defaultQueue()->poll();                         //Non Blocking
    }else
    {
        if( nTimeout == 0 )
        {
            status = Tibrv::defaultQueue()->dispatch();                 //Blocking
        }else
        {
            status = Tibrv::defaultQueue()->timedDispatch( nTimeout );  //Timed Blocking
        }
    }

    if ( m_rvRead == FALSE && status != TIBRV_OK )
    {
        if ( status != TIBRV_TIMEOUT )
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
        }

        return FALSE;
    }

    if ( status != TIBRV_OK )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::GetRvMessage) :: .. (!TIBRV_OK) : status=%d, text=%s", (int) status, status.getText());
    }

    // P R O C   R E C E I V E   R E N D E Z V O U S   M E S S A G E
    if ( m_rvRead == TRUE  && m_rvmessage.Length() > 0 )
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::GetRvMessage) :: RECV_RV : %s", m_rvmessage.String() );
        //Debug( "(RECV_RV) ::: '%s'", m_rvmessage.String());

        cszRvMsg    = m_rvmessage;
        eRvReadType = m_rvReadType;
        cszRvMsgBin = (eRvReadType & MESSAGE_BINARY)? m_rvmessageBinary: NULL;

        return TRUE;
    }
    else
        return FALSE;
}

int CRvFTListener::GetRvMessage ( CString &cszRvMsg, int &eRvReadType, int nTimeout /*= 0*/ )
{
    TibrvStatus status;

    // dispatch Tibrv events
    Clear();

    if ( m_BlockMode == RV_MSG_NON_BLOCK )
    {
        status = Tibrv::defaultQueue()->poll();                         //Non Blocking
    }else
    {
        if( nTimeout == 0 )
        {
            status = Tibrv::defaultQueue()->dispatch();                 //Blocking
        }else
        {
            status = Tibrv::defaultQueue()->timedDispatch( nTimeout );  //Timed Blocking
        }
    }

//Debug("GetRvMessage ::: dispatch()....end!!!");

    if ( m_rvRead == FALSE && status != TIBRV_OK )
    {
        if ( status != TIBRV_TIMEOUT )
        {
            if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::GetRvMessage) :: .. status=%d, text=%s", (int) status, status.getText());
        }

        return FALSE;
    }

    if ( status != TIBRV_OK )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::GetRvMessage) :: .. (!TIBRV_OK) : status=%d, text=%s", (int) status, status.getText());
    }

    // P R O C   R E C E I V E   R E N D E Z V O U S   M E S S A G E
    if ( m_rvRead == TRUE )  
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvFTListener::GetRvMessage) :: RECV_RV : %s", m_rvmessage.String() );
        //Debug( "(RECV_RV) ::: '%s'", m_rvmessage.String());

        eRvReadType = m_rvReadType;

        if ( eRvReadType & MESSAGE )  cszRvMsg    = m_rvmessage; // �޼����� type�� ����
        
        return TRUE;
    }
    else
        return FALSE;
}
// GetRvMessage �� ȣ���� ��, eRvReadType �� PPBODY, DMS �� ��� ������ �Ʒ� �� �Լ��� �̿��� Bin �� ���� �Ѵ�
CString CRvFTListener::GetBinBinary (  )
{
    return m_rvmessageBinary;
}
CString CRvFTListener::GetBinPPBODY (  )
{
    return m_rvmessagePPBODY;
}
CString CRvFTListener::GetBinDMS (  )
{
    return m_rvmessageDMS;
}
CString CRvFTListener::GetReplySubject (  )
{
    return m_rvReplySubject;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------
//  RV Main Class
//-----------------------------------------------------------------------------------------------------------------------------------------------
//CRvMain( CLogFile *pLog, RV_SENDER SenderFlag, RV_LISTEN ListenFlag, RV_BLOCK BlockMode )
CRvMain::CRvMain  ( CLogFile *pLog, RV_SENDER SenderFlag, RV_LISTEN ListenFlag, RV_BLOCK BlockMode, 
            RV_CAST CastMode, CString cszService, CString cszNetwork, CString cszDaemon )
{
    //Debug("CRvMain()");

    m_SenderFlag    = SenderFlag;
    m_ListenFlag    = ListenFlag;
    m_BlockMode     = BlockMode;

    m_CastMode      = CastMode;

    m_cszService    = cszService;
    m_cszNetwork    = cszNetwork;
    m_cszDaemon     = cszDaemon;

    mainLog         = pLog;

    m_pSender       = NULL;
    m_pListener     = NULL;
    m_pCMQListener  = NULL;     // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    m_pFTListener   = NULL;     // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�

}
CRvMain::~CRvMain()
{
    if ( m_pSender )
        delete m_pSender;

    if ( m_pListener )
        delete m_pListener;

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_pCMQListener )
        delete m_pCMQListener;

    // 2016.01.11 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_pFTListener )
        delete m_pFTListener;

    Tibrv::close();
    //Debug("~CRvMain()");
}

int CRvMain::m_rvOpen()
{
    TibrvStatus status;

    // open Tibrv
    status = Tibrv::open();
    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_rvOpen) :: could not open TIB/RV, status=%d, text=%s", (int) status, status.getText());
        return FALSE;
    }
    else
        return TRUE;
}

int CRvMain::m_Create_Transport( CString cszDescription )
{
    TibrvStatus status;

    // Create network transport 
    if ( m_CastMode == RV_BROADCAST )
    {
        status = m_Transport.create( NULL, NULL, NULL );  // (serviceStr,networkStr,daemonStr) 
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_Transport) :: transport_Create ::: No Option");
    }
    else // Multi casting
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_Transport) :: RV MULTI CASTING : Service='%s', Network='%s', Daemon='%s'", m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String());
        status = m_Transport.create( m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String() );  // (serviceStr,networkStr,daemonStr) 
    }

    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_Transport) :: transport_Create_Fail ::: status=%d, text=%s, ('%s', '%s', '%s')", (int)status, status.getText(), m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String() );
        return FALSE;
    }
    //else
    //{
    //    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_Transport) :: transport_Create_OK ::: ('%s', '%s', '%s')", m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String() );
    //}

    m_Transport.setDescription( cszDescription.String() );

    return TRUE;
}

int CRvMain::m_Create_FT_Transport( CString cszDescription )
{
    TibrvStatus status;

    // Create network transport 
    if ( m_CastMode == RV_BROADCAST )
    {
        status = m_FTTransport.create( NULL, NULL, NULL );  // (serviceStr,networkStr,daemonStr) 
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_FT_Transport) :: Create_FT_Transport ::: No Option");
    }
    else // Multi casting
    {
        //if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_FT_Transport) :: Create_FT_Transport :: RV MULTI CASTING : Service='%s', Network='%s', Daemon='%s'", m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String());
        status = m_FTTransport.create( m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String() );  // (serviceStr,networkStr,daemonStr) 
    }

    if (status != TIBRV_OK)
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_FT_Transport) :: Create_FT_Transport FAIL ::: status=%d, text=%s, ('%s', '%s', '%s')", (int)status, status.getText(), m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String() );
        return FALSE;
    }
    //else
    //{
    //    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::m_Create_FT_Transport) :: Create_FT_Transport OK ::: ('%s', '%s', '%s')", m_cszService.String(), m_cszNetwork.String(), m_cszDaemon.String() );
    //}

    m_FTTransport.setDescription( cszDescription.String() );

    return TRUE;
}

TibrvNetTransport * CRvMain::GetTransPort()
{
    return &m_Transport;
}

int CRvMain::Rv_Init( CString cszDescription, CString cszRvSubject, TibrvNetTransport *pTransport /*= NULL*/ )
{
    if ( m_rvOpen() == FALSE )
    {
        //Debug("RvOpen Error...");
        return FALSE;
    }

    if( pTransport == NULL )
    {
        int nRetryCnt = 0;

        while ( 1 )
        {
            if ( m_Create_Transport( cszDescription ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create Transport Error...");

                if ( ++nRetryCnt >= 3 )  // 3ȸ retry �ǽÿ��� transport ���� �ȵ�
                {
                    return FALSE;
                }

                usleep(100000);  //0.1 ��
            }
            else
                break;
        }

        if ( m_SenderFlag == RV_SENDER_ON )
        {
            m_pSender   = new CRvSender   ( mainLog, &m_Transport );
        }

        if ( m_ListenFlag == RV_LISTENER_ON )
        {
            m_pListener = new CRvListener ( mainLog, m_BlockMode, &m_Transport );

            if ( m_pListener->Create_Listner( cszRvSubject ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create Listner");
                return FALSE;
            }
        }

        // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
        if ( m_ListenFlag == RV_CMQLISTENER_ON )
        {
            m_pCMQListener = new CRvCMQListener ( mainLog, m_BlockMode, &m_Transport );

            if ( m_pCMQListener->Create_Listner( cszRvSubject, m_cszDQIdentifyID ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create CMQListner");
                return FALSE;
            }
        }

        // 2016.01.11 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
        if ( m_ListenFlag == RV_FTLISTENER_ON )
        {
            nRetryCnt = 0;
            while ( 1 )
            {
                if ( m_Create_FT_Transport( m_cszFTGroupName ) == FALSE )
                {
                    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create FT Transport Error...");

                    if ( ++nRetryCnt >= 3 )  // 3ȸ retry �ǽÿ��� transport ���� �ȵ�
                    {
                        return FALSE;
                    }

                    usleep(100000);  //0.1 ��
                }
                else
                    break;
            }

            m_pFTListener = new CRvFTListener ( mainLog, m_BlockMode, &m_Transport );

            m_pFTListener->Set_MySubject( cszRvSubject );

            if ( m_pFTListener->Create_FTMember( &m_FTTransport, m_cszFTGroupName, m_lnFTWeight, m_lnFTActiveGoalNum, 
                                                 m_dbFTHBInterval, m_dbFTPrepareInterval, m_dbFTActivateInterval ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create FT Listner");
                return FALSE;
            }
        }
    }
    else
    {    
        if ( m_SenderFlag == RV_SENDER_ON )
        {
            m_pSender   = new CRvSender   ( mainLog, pTransport );
        }

        if ( m_ListenFlag == RV_LISTENER_ON )
        {
            m_pListener = new CRvListener ( mainLog, m_BlockMode, pTransport );

            if ( m_pListener->Create_Listner( cszRvSubject ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create Listner");
                return FALSE;
            }
        }

        // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
        if ( m_ListenFlag == RV_CMQLISTENER_ON )
        {
            m_pCMQListener = new CRvCMQListener ( mainLog, m_BlockMode, pTransport );

            if ( m_pCMQListener->Create_Listner( cszRvSubject, m_cszDQIdentifyID ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create CMQListner");
                return FALSE;
            }
        }

        // 2016.01.11 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
        if ( m_ListenFlag == RV_FTLISTENER_ON )
        {
            int nRetryCnt = 0;

            while ( 1 )
            {
                if ( m_Create_FT_Transport( m_cszFTGroupName ) == FALSE )
                {
                    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create FT Transport Error...");
                    
                    if ( ++nRetryCnt >= 3 )  // 3ȸ retry �ǽÿ��� transport ���� �ȵ�
                    {
                        return FALSE;
                    }

                    usleep(100000);  //0.1 ��
                }
                else
                    break;
            }

            m_pFTListener = new CRvFTListener ( mainLog, m_BlockMode, &m_Transport );

            m_pFTListener->Set_MySubject( cszRvSubject );

            if ( m_pFTListener->Create_FTMember( &m_FTTransport, m_cszFTGroupName, m_lnFTWeight, m_lnFTActiveGoalNum, 
                                                 m_dbFTHBInterval, m_dbFTPrepareInterval, m_dbFTActivateInterval ) == FALSE )
            {
                if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Rv_Init) :: Can't Create FT Listner");
                return FALSE;
            }
        }
    }

    return TRUE;
}

 
 // long    ftweight = 50;
 // long    activeGoalNum = 1;
 // double  hbInterval = 1.5;
 // double  prepareInterval = 3; 
 // double  activateInterval = 4.8;
void CRvMain::Set_FT_RV_Info( CString cszGroupName, long nWeight, long nActiveGoalNum, 
                             double fHBInterval, double fPrepareInterval, double fActivateInterval  )
{
    // 2017.04.17 LKB : Rv DQ�� IdentifyID �� FT�� GroupName�� hostname �߰�
    char csHostName[256];
    if ( gethostname( csHostName, sizeof(csHostName)) < 0 )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Set_FT_RV_Info) :: gethostname()" );
        m_cszFTGroupName = cszGroupName;
    }else
    {
        m_cszFTGroupName.Format("%s_%s", cszGroupName.String(), csHostName);
    }

    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::Set_FT_RV_Info) :: FTGroupName=[%s]", m_cszFTGroupName.String());

    m_lnFTWeight            = nWeight;
    m_lnFTActiveGoalNum     = nActiveGoalNum;
    m_dbFTHBInterval        = fHBInterval;
    m_dbFTPrepareInterval   = fPrepareInterval;
    m_dbFTActivateInterval  = fActivateInterval;
}

void CRvMain::Set_DQ_RV_Info( CString cszIdentifyID )
{
    // 2017.05.19 LKB : Rv DQ�� IdentifyID ����
    char csHostName[256];
    if ( gethostname( csHostName, sizeof(csHostName)) < 0 )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::Set_DQ_RV_Info) :: gethostname()" );
        m_cszDQIdentifyID = cszIdentifyID;
    }else
    {
        m_cszDQIdentifyID.Format("%s_%s", cszIdentifyID.String(), csHostName);
    }

    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "(CRvMain::Set_DQ_RV_Info) :: DQIdentifyID=[%s]", m_cszDQIdentifyID.String());
}

//int CRvMain::GetRvMessage( CString &cszRvMsg,  RV_SEND_TYPE &eRvReadType, CString &cszRvMsgBin )
int CRvMain::GetRvMessage( CString &cszRvMsg,  int &eRvReadType, CString &cszRvMsgBin, int nTimeout /*= 0*/ )
{
    if ( m_ListenFlag == RV_LISTENER_OFF )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::GetRvMessage) :: Has not been created RvListener...");
        return FALSE;
    }

    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        return m_pListener->GetRvMessage( cszRvMsg, eRvReadType, cszRvMsgBin, nTimeout );
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        return m_pCMQListener->GetRvMessage( cszRvMsg, eRvReadType, cszRvMsgBin, nTimeout );
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        return m_pFTListener->GetRvMessage( cszRvMsg, eRvReadType, cszRvMsgBin, nTimeout );
    }

    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::GetRvMessage) :: Undefined ListenFlag ...");
    return FALSE;
}

int CRvMain::GetRvMessage( CString &cszRvMsg,  int &eRvReadType, int nTimeout /*= 0*/ )
{
    if ( m_ListenFlag == RV_LISTENER_OFF )
    {
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::GetRvMessage) :: Has not been created RvListener...");
        return FALSE;
    }

    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        return m_pListener->GetRvMessage( cszRvMsg, eRvReadType, nTimeout );
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        return m_pCMQListener->GetRvMessage( cszRvMsg, eRvReadType, nTimeout );
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        return m_pFTListener->GetRvMessage( cszRvMsg, eRvReadType, nTimeout );
    }

    if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::GetRvMessage) :: Undefined ListenFlag ...");
    return FALSE;
}

// GetRvMessage �� ȣ���� ��, eRvReadType �� PPBODY, DMS �� ��� ������ �Ʒ� �� �Լ��� �̿��� Bin �� ���� �Ѵ�
CString CRvMain::GetBinData ( int nRvReadType )
{
    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        switch ( nRvReadType )
        {
            case MESSAGE_BINARY:    return m_pListener->GetBinBinary();
            case MESSAGE_PPBODY:    return m_pListener->GetBinPPBODY();
            case MESSAGE_DMS:       return m_pListener->GetBinDMS();

            default :               return CString("");
        }
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        switch ( nRvReadType )
        {
            case MESSAGE_BINARY:    return m_pCMQListener->GetBinBinary();
            case MESSAGE_PPBODY:    return m_pCMQListener->GetBinPPBODY();
            case MESSAGE_DMS:       return m_pCMQListener->GetBinDMS();

            default :               return CString("");
        }
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        switch ( nRvReadType )
        {
            case MESSAGE_BINARY:    return m_pFTListener->GetBinBinary();
            case MESSAGE_PPBODY:    return m_pFTListener->GetBinPPBODY();
            case MESSAGE_DMS:       return m_pFTListener->GetBinDMS();

            default :               return CString("");
        }
    }

    return CString("");
}

//int CRvMain::SendRvMsg( CString cszToSubject, CString cszSendMsg, RV_SEND_TYPE ervSendType, CString cszSendMsgBinary )
int CRvMain::SendRvMsg( CString cszToSubject, CString cszSendMsg, int ervSendType, CString cszSendMsgBinary, CString cszSendMsgPPBODY, CString cszSendMsgDMS )
{
    if ( m_SenderFlag == RV_SENDER_OFF )
    {
        //Debug("has not been created RvSender...");
        if ( mainLog != NULL ) mainLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "(CRvMain::SendRvMsg) :: Has not been created RvSender...");
        return FALSE;
    }

    return m_pSender->SendRvMsg( cszToSubject, cszSendMsg, ervSendType, cszSendMsgBinary, cszSendMsgPPBODY, cszSendMsgDMS );
}


////
//2006-12-24 ::: RV Config �� �о��
void GetRvConfig( RV_CAST &eCastMode, CString &cszService, CString &cszNetwork, CString &cszDaemon )
{
    char szEnv[128];
    // enum RV_CAST   { RV_BROADCAST = 0,    RV_MULTICAST     };

    strcpy( szEnv, getenv("RVCASTMODE"));

    if ( !strncmp(szEnv, "BROAD", 5) )
    {
            eCastMode = RV_BROADCAST;

            cszService = (char *)NULL;
            cszNetwork = (char *)NULL;
            cszDaemon  = (char *)NULL;

            return;
    }

    // MULTI CAST �̸� Option �� �о��
    eCastMode = RV_MULTICAST;

    strcpy( szEnv, getenv("RVOPTION"));
    cszService = strtok( szEnv, "^");
    cszNetwork = strtok( NULL , "^");
    cszDaemon  = strtok( NULL , "^\n");
}

////
//2006-12-24 ::: RV Config �� �о��
void GetSimaxRvConfig( CString &cszService, CString &cszNetwork, CString &cszDaemon )
{
    char szEnv[128];

    strcpy( szEnv, getenv("SIMAXRVOPTION"));
    cszService = strtok( szEnv, "^");
    cszNetwork = strtok( NULL , "^");
    cszDaemon  = strtok( NULL , "^\n");
}

CString CRvMain::GetBinBinary (  )
{
    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        return m_pListener->GetBinBinary();
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        return m_pCMQListener->GetBinBinary();
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        return m_pFTListener->GetBinBinary();
    }

    return CString("");
}

CString CRvMain::GetBinPPBODY (  )
{
    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        return m_pListener->GetBinPPBODY();
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        return m_pCMQListener->GetBinPPBODY();
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        return m_pFTListener->GetBinPPBODY();
    }

    return CString("");
}

CString CRvMain::GetBinDMS (  )
{
    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        return m_pListener->GetBinDMS();
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        return m_pCMQListener->GetBinDMS();
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        return m_pFTListener->GetBinDMS();
    }

    return CString("");
}

CString CRvMain::GetReplySubject (  )
{
    if ( m_ListenFlag == RV_LISTENER_ON )
    {
        return m_pListener->GetReplySubject();
    }

    // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    if ( m_ListenFlag == RV_CMQLISTENER_ON )
    {
        return m_pCMQListener->GetReplySubject();
    }

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance) ��� �߰�
    if ( m_ListenFlag == RV_FTLISTENER_ON )
    {
        return m_pFTListener->GetReplySubject();
    }

    return CString("");
}
