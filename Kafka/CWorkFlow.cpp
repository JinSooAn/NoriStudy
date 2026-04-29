/*
This software is the intellectual property and confidential information of Samsung Electronics, Co., Ltd. (Samsung) protected by applicable laws. 
The unauthorized use or disclosure of the software to any third party without the prior written consent of Samsung is strictly prohibited, 
and you should be legally responsible for the unauthorized use or violation of confidentiality obligation.   
*/

#include "CWorkflow.hpp"
#include "COracle.hpp"
#include "CKafka.hpp"

extern char *DEFAULT_LOG_PATH;
extern char *DEFAULT_LOG_FILE;
extern char *DEFAULT_LOG_FILE_QA;
extern char *DEFAULT_TMP_PATH;

extern char *DEFAULT_RV_SERVICE;
extern char *DEFAULT_RV_NETWORK;
extern char *DEFAULT_RV_DAEMON;

extern char *DEFAULT_RV_FDC_SERVICE;
extern char *DEFAULT_RV_FDC_NETWORK;
extern char *DEFAULT_RV_FDC_DAEMON;

extern char *DEFAULT_RV_EES_SERVICE;
extern char *DEFAULT_RV_EES_NETWORK;
extern char *DEFAULT_RV_EES_DAEMON;

extern char *DEFAULT_RV_SPC_SERVICE;
extern char *DEFAULT_RV_SPC_NETWORK;
extern char *DEFAULT_RV_SPC_DAEMON;

extern char *DEFAULT_RV_MES_SERVICE;
extern char *DEFAULT_RV_MES_NETWORK;
extern char *DEFAULT_RV_MES_DAEMON;

extern char *DEFAULT_RV_TIMEOUT;

extern char *DEFAULT_RV_WORKFLOW_SUBJECT;
extern char *DEFAULT_RV_SOCKETREMOTE_SUBJECT;
extern char *DEFAULT_RV_EQP_SUBJECT;

extern char *DEFAULT_RV_MES_SUBJECT;
extern char *DEFAULT_RV_WORKMAN_SUBJECT;
extern char *DEFAULT_RV_MES_LISTEN;
extern char *DEFAULT_RV_MES_LISTEN_HDR;

extern char *DEFAULT_RV_ANOMALY_SUBJECT;

extern char *DEFAULT_RV_FDC_HDR;
extern char *DEFAULT_RV_EES_SUBJECT;
extern char *DEFAULT_RV_SPC_SUBJECT;

extern char *DEFAULT_RV_MCTS_SERVICE;
extern char *DEFAULT_RV_MCTS_NETWORK;
extern char *DEFAULT_RV_MCTS_DAEMON;
extern char *DEFAULT_RV_MCTS_SUBJECT;
extern char *DEFAULT_RV_MCTS_LISTEN;
extern char *DEFAULT_RV_MCTS_LISTEN_HDR;

CWorkflow::CWorkflow(CIniFile *pTEnv, CLogFile *pTLog, TibrvNetTransport *pTransport)
{
    m_pTEnv = pTEnv;
    m_pTLog = pTLog;

    m_TQALog.SetLogFile( m_pTEnv->GetString( "SYSTEM", "LOG_FILE_QA", DEFAULT_LOG_FILE_QA )  );
    m_TQALog.SetLogUnit( "HOUR" );

    m_TOra.SetLogFile( pTLog );

    m_TReplyMsg = "";
    
    CString TRvService, TRvNetwork, TRvDaemon;
    RV_CAST eRvType = RV_MULTICAST;

    TRvService = m_pTEnv->GetString("RV_CONF", "SERVICE", DEFAULT_RV_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "NETWORK", DEFAULT_RV_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "DAEMON" , DEFAULT_RV_DAEMON );

    // Local Process I/F RV
    m_pTRv = new CRvMain(pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

#ifdef __EMUL__
    m_TRvSubject.Format((char *)"%s.TEST", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT));
#else    
    m_TRvSubject.Format((char *)"%s.%d_%s", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid(), GetDateTime_YyyyMmDdHhMiSsCc() + 8 );
#endif
    if( m_pTRv->Rv_Init( m_TRvSubject, m_TRvSubject ) == FALSE )
    {   
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "RV INIT ERROR");
    }

    // EqpAgent I/F RV
    m_pTEqpRv = new CRvMain(pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );
    m_pDRT = NULL;

#ifdef __EMUL__
    m_TEqpRvSubject.Format((char *)"%s.EQP", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT));
#else    
    m_TEqpRvSubject.Format((char *)"%s.EQP%d_%s", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid(), GetDateTime_YyyyMmDdHhMiSsCc() + 8);
#endif
    if( m_pTEqpRv->Rv_Init( m_TEqpRvSubject, m_TEqpRvSubject, m_pTRv->GetTransPort() ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "RV INIT ERROR");
    }

    m_pTMOS_Rv = NULL;    // 2020-09-21
    m_TRvMosSubject.Format   ((char *)"%s%d", m_pTEnv->GetString("RV_SUBJECT", "MES_LISTEN",     DEFAULT_RV_MES_LISTEN), getpid() );
    m_TRvMosSubjectHDR.Format((char *)"%s%d", m_pTEnv->GetString("RV_SUBJECT", "MES_LISTEN_HDR", DEFAULT_RV_MES_LISTEN_HDR), getpid() );

    m_TTmpPath  = m_pTEnv->GetString("SYSTEM", "TMP_PATH" , DEFAULT_TMP_PATH );
}

CWorkflow::~CWorkflow()
{
    if( m_TOra.DisConnectDB() == FALSE ) 
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "DB DisConnectDB ERROR.");
    }else
    {
        m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "DB DisConnectDB.");
    }

    if( m_pTRv      )   delete m_pTRv;          m_pTRv      = NULL;
    if( m_pTEqpRv   )   delete m_pTEqpRv;       m_pTEqpRv   = NULL;
    if( m_pTMOS_Rv  )   delete m_pTMOS_Rv;      m_pTMOS_Rv  = NULL;
    if( m_pTFDC_Rv  )   delete m_pTFDC_Rv;      m_pTFDC_Rv  = NULL;
    if( m_pTRMM_Rv  )   delete m_pTRMM_Rv;      m_pTRMM_Rv  = NULL;
    if( m_pDRT     )   delete m_pDRT;           m_pDRT      = NULL;
}

bool CWorkflow::CheckPara(CFilter &TFilter, CString TParaName, CString &TErrorMsg)
{
    if( TFilter.IsArg(TParaName.GetPrintString()) == 0 || TFilter.GetArg(TParaName.GetPrintString()) == NULL )
    {
        TErrorMsg.Format((char *)"%s Not Found", TParaName.GetPrintString());
        return FALSE;
    }
    
    return TRUE;
}

bool CWorkflow::AddMsg(CFilter &TMsgFilter, CString &TMsg, CString TDataName, CString TMsgName, CString TLeftBrace /*= ""*/, CString TRightBrace /*= ""*/)
{
    CString TBuf;
    
    if( TMsgFilter.GetArg(TDataName.GetPrintString()) != NULL ) 
    {
        TBuf.Format((char *)" %s=%s%s%s", TMsgName.GetPrintString(), TLeftBrace.GetPrintString(), TMsgFilter.GetArg(TDataName.GetPrintString()), TRightBrace.GetPrintString() );
        TMsg += TBuf;
    }
    
    return TRUE;
}

CString CWorkflow::GetEqpSubject(CString TEqpID)
{
    CString TSubject;
    
    TSubject.Format((char *)"%s.%s", 
            m_pTEnv->GetString("RV_SUBJECT", "EQP", DEFAULT_RV_EQP_SUBJECT),
            TEqpID.GetPrintString() );
            
    return TSubject;
}

CString CWorkflow::GetEqpSubject_Sub(CString TEqpID)
{
    CString TSubject;
    
    TSubject.Format((char *)"%s.%s", 
            m_pTEnv->GetString("RV_SUBJECT", "EQP_SUB", DEFAULT_RV_EQP_SUBJECT),
            TEqpID.GetPrintString() );
            
    return TSubject;
}

CString CWorkflow::GetRPTInfo(CFilter &TFilter, CString TItem1, CString TItem2 /*= ""*/)
{
    CString TRPTName;

    if( TFilter.IsArg( (char *)"RPTINFO", TItem1.GetPrintString()) == 0 || TFilter.GetArg( (char *)"RPTINFO", TItem1.GetPrintString()) == NULL )
    {
        TRPTName = "";
//        m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "CWorkflow::GetRPTInfo : 1 [%s] [%s]", TRPTName.GetPrintString(), TItem1.GetPrintString());
    }else
    {
        TRPTName = TFilter.GetArg( (char *)"RPTINFO", TItem1.GetPrintString());
//        m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "CWorkflow::GetRPTInfo : 2 [%s] [%s]", TRPTName.GetPrintString(), TItem1.GetPrintString());
    }

    if ( TRPTName.Length() == 0 && TItem2.Length() > 0 )
    {
        if( TFilter.IsArg( (char *)"RPTINFO", TItem2.GetPrintString()) == 0 || TFilter.GetArg( (char *)"RPTINFO", TItem2.GetPrintString()) == NULL )
        {
            TRPTName = "";
        }else
        {
            TRPTName = TFilter.GetArg( (char *)"RPTINFO", TItem2.GetPrintString());
        }
    }

    if ( TRPTName.Length() == 0)
    {
        TRPTName = TItem1;
    }

    return TRPTName;
}

CString CWorkflow::GetBinData (  )
{
    return m_TBinData;
}
CString CWorkflow::GetBodyData (  )
{
    return m_TBodyData;
}
CString CWorkflow::GetDmsData (  )
{
    return m_TDmsData;
}

bool CWorkflow::StartSubMessage(char *pszName)
{
    CString TBuf;
    
    TBuf.Format((char *)"%s=(", pszName);
    m_TReplyMsg += TBuf;
    
    return TRUE;
}

bool CWorkflow::EndSubMessage()
{
    CString TBuf;
    
    TBuf = ") ";
    m_TReplyMsg += TBuf;
    
    return TRUE;
}

bool CWorkflow::MakeMessage(char *pszName, char *pszValue, int nLen /*= 0*/)
{
    if( nLen == 0 )
    {
        CString TBuf;
        
        TBuf.Format((char *)"%s=%s ", (pszName == NULL) ? "" : pszName, (pszValue == NULL) ? "" : pszValue);
        m_TReplyMsg += TBuf;
    }
    else
    {
        char *pszBuffer = new char[ nLen + 1 ];
        
        memset(pszBuffer, 0, nLen + 1);
        if( pszValue ) memcpy(pszBuffer, pszValue, nLen);

        CString TBuf;
        
        TBuf.Format((char *)"%s=%s ", (pszName == NULL) ? "" : pszName, pszBuffer);
        m_TReplyMsg += TBuf;
        
        delete[] pszBuffer; pszBuffer = NULL;
    }
    
    return TRUE;
}

bool CWorkflow::SetMsgSeq(CString TMsgSeq)
{
    m_TMsgSeq   = TMsgSeq;

    return TRUE;
}

CString CWorkflow::GetFDCSubject()
{
    CString TSubject;
    TSubject.Format("%s",m_pTEnv->GetString("RV_SUBJECT", "FDC_HDR", DEFAULT_RV_FDC_HDR) );

    return TSubject;
}

CString CWorkflow::GetArgfromMES( CFilter &TFilter, CString TItemName, CString TItemName2 /*= ""*/)
{
    CString TValue;
    TValue  = "";

    if( TFilter.GetArg(TItemName.GetPrintString()) != NULL )
    {
        TValue = TFilter.GetArg(TItemName.GetPrintString());
    }
    else
    {
        if ( TItemName2.Length() > 0 )
        {
            TValue = TFilter.GetArg(TItemName.GetPrintString(), TItemName2.GetPrintString());
        }else
        {
            CDataLink *pTNode = TFilter.GetArgList(TItemName.GetPrintString());
            if( pTNode != NULL ) TValue = TFilter.GetArgName(0, pTNode);
        }
    }

    return TValue;
}

bool CWorkflow::EQP_Send(CString TSubject, CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= 0*/)
{
    CRvMain*    pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    RV_CAST eRvType = RV_MULTICAST;

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    ptr = strstr(TMsg.GetPrintString(), " EQP_INFO=(");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    CString TBuf;

    if( nWaitReply == 1 )
    {
        ptr = strstr(TMsg.GetPrintString(), " REPLY=");
        if( !ptr )
        {
            TBuf.Format((char *)" REPLY=Y");
            TMsg += TBuf;
        }
    }

    ptr = strstr(TMsg.GetPrintString(), " MSG_SEQ=");
    if( !ptr )
    {
        TBuf.Format((char *)" MSG_SEQ=%s", m_TMsgSeq.GetPrintString());
        TMsg += TBuf;
    }

    TBuf.Format((char *)" FROM=%s", m_TEqpRvSubject.GetPrintString());
    TMsg += TBuf;

    TBuf.Format((char *)" TO=%s", TSubject.GetPrintString());
    TMsg += TBuf;

    CFilter TFilter(TMsg.GetPrintString());

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "SERVICE", DEFAULT_RV_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "NETWORK", DEFAULT_RV_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "DAEMON" , DEFAULT_RV_DAEMON );

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

    TBuf.Format((char *)"%s.TMP%d_%s", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid(), GetDateTime_YyyyMmDdHhMiSsCc() + 8 );
    if( pTRv->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "EQP SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( TSubject, TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_EQP, eTAB1, "%s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);
        m_TQALog.WriteQA("[%s][EQP Send][%s]", TFilter.GetArg((char *)"EQPID"), TMsg.GetPrintString());
    }
    else
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);
    }

    if( pTRv ) delete pTRv; pTRv = NULL;
    
    if( nWaitReply == 1 )
    {
        CString TRvMsg;
        int     nRvMsgType;
        
        m_TBinData     = "";
        m_TBodyData    = "";
        m_TDmsData     = "";

        nRet = m_pTEqpRv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );
        
        if( nRet == TRUE )
        {
            m_TBinData     = m_pTEqpRv->GetBinData( MESSAGE_BINARY );
            m_TBodyData    = m_pTEqpRv->GetBinData( MESSAGE_PPBODY );
            m_TDmsData     = m_pTEqpRv->GetBinData( MESSAGE_DMS    );
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_EQP, eTAB1, "%s", TRvMsg.GetPrintString());
            m_TQALog.WriteQA("[%s][EQP Recv][%s]", TFilter.GetArg((char *)"EQPID"), TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, eTAB1, "RECV NG : TIMEOUT=%s", m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT));
        }

        TRetMsg = TRvMsg;
    }

    return nRet;
}

bool CWorkflow::FDC_Send(CString TSubject, CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg)
{
    CRvMain*    pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    CString TBuf;
    RV_CAST eRvType = RV_MULTICAST;

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "FDC_SERVICE", DEFAULT_RV_FDC_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "FDC_NETWORK", DEFAULT_RV_FDC_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "FDC_DAEMON" , DEFAULT_RV_FDC_DAEMON );

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

    TBuf.Format((char *)"%s.%d", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid() );

    if( pTRv->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_FDC, eTAB1, "FDC SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( TSubject, TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_FDC, eTAB1, "%s : TYPE=[%d] TO=[%s]", TMsg.GetPrintString(), nRvType, TSubject.GetPrintString() );
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_FDC, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);

    if( pTRv ) delete pTRv; pTRv = NULL;

    return TRUE;
}

bool CWorkflow::EES_Send(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg)
{
    CRvMain*    pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    CString TBuf;
    RV_CAST eRvType = RV_MULTICAST;

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "EES_SERVICE", DEFAULT_RV_EES_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "EES_NETWORK", DEFAULT_RV_EES_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "EES_DAEMON" , DEFAULT_RV_EES_DAEMON );

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

    TBuf.Format((char *)"%s.%d", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid() );

    if( pTRv->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "EES SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT), TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_EES, eTAB1, "%s : TYPE=[%d] TO=[%s]", TMsg.GetPrintString(), nRvType, m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT));
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);

    if( pTRv ) delete pTRv; pTRv = NULL;

    return TRUE;
}

bool CWorkflow::SPC_Send(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg)
{
    CRvMain*    pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    CString TBuf;
    RV_CAST eRvType = RV_MULTICAST;

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "SPC_SERVICE", DEFAULT_RV_SPC_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "SPC_NETWORK", DEFAULT_RV_SPC_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "SPC_DAEMON" , DEFAULT_RV_SPC_DAEMON );

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

    TBuf.Format((char *)"%s.%d", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid() );

    if( pTRv->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "SPC SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( m_pTEnv->GetString("RV_SUBJECT", "SPC", DEFAULT_RV_SPC_SUBJECT), TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_EES, eTAB1, "%s : TYPE=[%d] TO=[%s]", TMsg.GetPrintString(), nRvType, m_pTEnv->GetString("RV_SUBJECT", "SPC", DEFAULT_RV_SPC_SUBJECT));
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);

    if( pTRv ) delete pTRv; pTRv = NULL;

    return TRUE;
}


bool CWorkflow::EES_Send_Wait(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, CString &TEqpID, int nWaitReply /*= 0*/)
{
    CRvMain*    pTRvSender;
    CRvMain*    pTRvListener;
    CString     TRvService, TRvNetwork, TRvDaemon;
    CString     TBuf;
    int         nRet;
    RV_CAST     eRvType = RV_MULTICAST;

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "EES_SERVICE", DEFAULT_RV_EES_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "EES_NETWORK", DEFAULT_RV_EES_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "EES_DAEMON" , DEFAULT_RV_EES_DAEMON );

    pTRvSender = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_OFF, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );    

    TBuf.Format((char *)"%s.%d", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid() );

    if( pTRvSender->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "EES SEND RV INIT");

        if( pTRvSender ) delete pTRvSender; pTRvSender = NULL;
        return FALSE;
    }

    TRvService = m_pTEnv->GetString("RV_CONF", "SERVICE", DEFAULT_RV_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "NETWORK", DEFAULT_RV_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "DAEMON" , DEFAULT_RV_DAEMON );

    pTRvListener = new CRvMain(m_pTLog, RV_SENDER_OFF, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );    

    TBuf.Format((char *)"%s.%s", m_pTEnv->GetString("RV_SUBJECT", "EQP", DEFAULT_RV_EQP_SUBJECT), TEqpID.GetPrintString());

    if( pTRvListener->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "EES SEND RV INIT");

        if( pTRvSender   ) delete pTRvSender;   pTRvSender   = NULL;
        if( pTRvListener ) delete pTRvListener; pTRvListener = NULL;
        return FALSE;
    }

    nRet = pTRvSender->SendRvMsg ( m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT), TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_EES, eTAB1, "%s : TYPE=[%d] TO=[%s]", TMsg.GetPrintString(), nRvType, m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT));
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);

    if( nWaitReply == TRUE )
    {
        CString TRvMsg;
        int     nRvMsgType;
        
        m_TBinData     = "";
        m_TBodyData    = "";
        m_TDmsData     = "";

        nRet = pTRvListener->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );
        
        if( nRet == TRUE )
        {
            m_TBinData     = pTRvListener->GetBinData( MESSAGE_BINARY );
            m_TBodyData    = pTRvListener->GetBinData( MESSAGE_PPBODY );
            m_TDmsData     = pTRvListener->GetBinData( MESSAGE_DMS    );
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_EES, eTAB1, "%s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EES, eTAB1, "RECV NG");

            if( pTRvSender   ) delete pTRvSender;   pTRvSender   = NULL;
            if( pTRvListener ) delete pTRvListener; pTRvListener = NULL;
            return FALSE;
        }
        TRetMsg = TRvMsg;
    }

    if( pTRvSender   ) delete pTRvSender;   pTRvSender   = NULL;
    if( pTRvListener ) delete pTRvListener; pTRvListener = NULL;

    return TRUE;
}

bool CWorkflow::Inkless_Comm(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= 0*/)
{
    CRvMain* pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    CString TListenSubject, TTargetSubject;
    CString TBuf;
    RV_CAST eRvType = RV_MULTICAST;

    TListenSubject.Format((char *)"%s.TMP%d_%s", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid(), GetDateTime_YyyyMmDdHhMiSsCc() + 8 );
    TTargetSubject = m_pTEnv->GetString("RV_SUBJECT", "INKLESS", "OYCP.INKLESS.MAP_AGENT");

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    ptr = strstr(TMsg.GetPrintString(), " EQP_INFO=(");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }
    
    if( nWaitReply == TRUE )
    {
        ptr = strstr(TMsg.GetPrintString(), " REPLY=");
        if( !ptr )
        {
            TBuf.Format((char *)" REPLY=Y");
            TMsg += TBuf;
        }
    }

    ptr = strstr(TMsg.GetPrintString(), " MSG_SEQ=");
    if( !ptr )
    {
        TBuf.Format((char *)" MSG_SEQ=%s", m_TMsgSeq.GetPrintString());
        TMsg += TBuf;
    }

    TBuf.Format((char *)" FROM=%s", TListenSubject.GetPrintString());
    TMsg += TBuf;

    TBuf.Format((char *)" TO=%s", TTargetSubject.GetPrintString());
    TMsg += TBuf;

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "INKLESS_SERVICE", DEFAULT_RV_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "INKLESS_NETWORK", DEFAULT_RV_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "INKLESS_DAEMON" , DEFAULT_RV_DAEMON );

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );    

    if( pTRv->Rv_Init( TListenSubject, TListenSubject ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_INKLESS, eTAB1, "INKLESS SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( TTargetSubject, TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_INKLESS, eTAB1, "%s : TYPE=[%d] TO=[%s]", TMsg.GetPrintString(), nRvType, TTargetSubject.GetPrintString() );
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_INKLESS, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);

    if( nWaitReply == TRUE )
    {
        CString TRvMsg;
        int     nRvMsgType;

        m_TBinData     = "";
        m_TBodyData    = "";
        m_TDmsData     = "";

        nRet = pTRv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );

        if( nRet == TRUE )
        {
            m_TBinData     = pTRv->GetBinData( MESSAGE_BINARY );
            m_TBodyData    = pTRv->GetBinData( MESSAGE_PPBODY );
            m_TDmsData     = pTRv->GetBinData( MESSAGE_DMS    );
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_INKLESS, eTAB1, "%s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_INKLESS, eTAB1, "RECV NG");

            if( pTRv ) delete pTRv; pTRv = NULL;
            return FALSE;
        }

        TRetMsg = TRvMsg;
    }

    if( pTRv ) delete pTRv; pTRv = NULL;

    return TRUE;
}

bool CWorkflow::Etc_Comm(CString &TMsg, CString TKind, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= 0*/)
{
    CRvMain* pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    CString TListenSubject, TTargetSubject;
    CString TBuf, TTemp;
    RV_CAST eRvType = RV_MULTICAST;

    TListenSubject.Format((char *)"%s.TMP%d_%s", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid(), GetDateTime_YyyyMmDdHhMiSsCc() + 8 );


    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    ptr = strstr(TMsg.GetPrintString(), " EQP_INFO=(");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }
    
    if( nWaitReply == TRUE )
    {
        ptr = strstr(TMsg.GetPrintString(), " REPLY=");
        if( !ptr )
        {
            TBuf.Format((char *)" REPLY=Y");
            TMsg += TBuf;
        }
    }

    ptr = strstr(TMsg.GetPrintString(), " MSG_SEQ=");
    if( !ptr )
    {
        TBuf.Format((char *)" MSG_SEQ=%s", m_TMsgSeq.GetPrintString());
        TMsg += TBuf;
    }

    TBuf.Format((char *)" FROM=%s", TListenSubject.GetPrintString());
    TMsg += TBuf;


    TBuf.Format((char *)" REPLY_SUBJECT=%s", TListenSubject.GetPrintString());
    TMsg += TBuf;

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;
    

    TTemp.Format("ETC_%s_SERVICE", TKind.GetPrintString());
    TRvService = m_pTEnv->GetString("RV_CONF", TTemp.GetPrintString(), DEFAULT_RV_SERVICE);
    
    TTemp.Format("ETC_%s_NETWORK", TKind.GetPrintString());
    TRvNetwork = m_pTEnv->GetString("RV_CONF", TTemp.GetPrintString(), DEFAULT_RV_NETWORK);

    TTemp.Format("ETC_%s_DAEMON", TKind.GetPrintString());
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", TTemp.GetPrintString(), DEFAULT_RV_DAEMON );
    
    TTemp.Format("ETC_%s_SUBJECT", TKind.GetPrintString());
    TTargetSubject = m_pTEnv->GetString("RV_SUBJECT", TTemp.GetPrintString(), "-" );


    TBuf.Format((char *)" TO=%s", TTargetSubject.GetPrintString());
    TMsg += TBuf;


    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );    

    if( pTRv->Rv_Init( TListenSubject, TListenSubject ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_INKLESS, eTAB1, "ETC SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( TTargetSubject, TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_ETC, eTAB1, "%s : TYPE=[%d] TO=[%s]", TMsg.GetPrintString(), nRvType, TTargetSubject.GetPrintString() );
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_ETC, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);

    if( nWaitReply == TRUE )
    {
        CString TRvMsg;
        int     nRvMsgType;

        m_TBinData     = "";
        m_TBodyData    = "";
        m_TDmsData     = "";

        TTemp.Format("ETC_%s_TIMEOUT", TKind.GetPrintString());

        nRet = pTRv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", TTemp.GetPrintString(), DEFAULT_RV_TIMEOUT)) );

        if( nRet == TRUE )
        {
            m_TBinData     = pTRv->GetBinData( MESSAGE_BINARY );
            m_TBodyData    = pTRv->GetBinData( MESSAGE_PPBODY );
            m_TDmsData     = pTRv->GetBinData( MESSAGE_DMS    );
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_INKLESS, eTAB1, "%s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_INKLESS, eTAB1, "RECV NG");

            if( pTRv ) delete pTRv; pTRv = NULL;
            return FALSE;
        }
        TRetMsg = TRvMsg;
    }

    if( pTRv ) delete pTRv; pTRv = NULL;

    return TRUE;
}

bool CWorkflow::MCTS_Comm(CString TCmd, CString TData, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= TRUE*/)
{
    CRvMain*    pTRv;
    CString TMsg;
    CString TRvService, TRvNetwork, TRvDaemon;
    CString TListenSubject, TTargetSubject, TTRvSubjectHDR;
    RV_CAST eRvType = RV_MULTICAST;

    TListenSubject.Format((char *)"%s%d", m_pTEnv->GetString("RV_SUBJECT", "MCTS_LISTEN", DEFAULT_RV_MCTS_LISTEN), getpid() );
    TTargetSubject = m_pTEnv->GetString("RV_SUBJECT", "MCTS", DEFAULT_RV_MCTS_SUBJECT);
    TTRvSubjectHDR.Format((char *)"%s%d", m_pTEnv->GetString("RV_SUBJECT", "MCTS_LISTEN_HDR", DEFAULT_RV_MCTS_LISTEN_HDR), getpid() );

    CFilter TFilter(TData.GetPrintString());

    TMsg.Format((char *)"%s HDR=(MCTS,%s,%s) %s REQ_SYSTEM=TC ",
                    TCmd.GetPrintString(),
                    TTRvSubjectHDR.GetPrintString(),
                    TCmd.GetPrintString(),
                    TData.GetPrintString() );

    TRvService = m_pTEnv->GetString("RV_CONF", "MCTS_SERVICE", DEFAULT_RV_MCTS_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "MCTS_NETWORK", DEFAULT_RV_MCTS_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "MCTS_DAEMON" , DEFAULT_RV_MCTS_DAEMON );

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

    if( pTRv->Rv_Init( TListenSubject, TListenSubject ) == FALSE )
    {
        if( pTRv ) delete pTRv; pTRv = NULL;
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "MCTS RV INIT ERROR");
        TRetMsg = "MCTS RV INIT ERROR";
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( TTargetSubject, TMsg );

    if( nRet == TRUE )
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_MCTS, eTAB1, "%s : TO=[%s]", TMsg.GetPrintString(), TTargetSubject.GetPrintString() );
    else
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_MCTS, eTAB1, "SEND NG : %s : TO=[%s]", TMsg.GetPrintString(), TTargetSubject.GetPrintString() );

    if( nRet == FALSE )
    {
        if( pTRv ) delete pTRv; pTRv = NULL;
        TRetMsg.Format((char *)"MCTS REPLY SEND NG. %s", strerror(errno));
        return FALSE;
    }

    if( nWaitReply == TRUE )
    {
        CString TRvMsg;
        int     nRvMsgType;

        nRet = pTRv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );
        
        if( nRet == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_MCTS, eTAB1, "RECV : %s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_MCTS, eTAB1, "%s", TRvMsg.GetPrintString());
        }

        if( nRet == FALSE )
        {
            if( pTRv ) delete pTRv; pTRv = NULL;
            TRetMsg = "MCTS REPLY TIMEOUT";
            return FALSE;
        }

        TRetMsg = TRvMsg;
    }

    if( pTRv ) delete pTRv; pTRv = NULL;

    return TRUE;
}

bool CWorkflow::MES_Comm_OiSocket(CString TTarIP, CString TTarPort, CString &TMsg, CString &TReplyMsg, int *npTimeout /*= NULL*/)
{
    char *sMsg = NULL;
    long lnLen, lnTotalLen;

    lnLen = TMsg.Length();
    if( lnLen < 1 )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "MES_Comm_OiSocket :: Message Length is Zero[%d] Error", lnLen );
        return FALSE;
    }

    sMsg = new char[ lnLen + 17 ];
    memset(sMsg, 0, lnLen + 17);

    lnTotalLen = sprintf(sMsg, "%16ld%s", lnLen, TMsg.GetPrintString());

    int nRet;

    m_pTSocket = new CSocket( m_pTLog->GetFileName() );

    nRet = m_pTSocket->Connect ( TTarIP.GetPrintString(), atoi(TTarPort.GetPrintString()) );

    if( npTimeout && *npTimeout )
    {
        TReplyMsg.Format((char *)"Simax Socket Connect TIMEOUT.");
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "Simax Socket Connect %s, %s TIMEOUT", TTarIP.GetPrintString(), TTarPort.GetPrintString());
        if( sMsg ) delete[] sMsg; sMsg = NULL;
        delete m_pTSocket;
        return FALSE;
    }
    if( nRet == FALSE )
    {
        TReplyMsg.Format((char *)"Simax Socket Connect Error.");
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "Simax Socket Connect %s, %s Error : %s", TTarIP.GetPrintString(), TTarPort.GetPrintString(), strerror(errno));
        if( sMsg ) delete[] sMsg; sMsg = NULL;
        delete m_pTSocket;
        return FALSE;
    }

    nRet = m_pTSocket->Write(sMsg, lnTotalLen);
    if( npTimeout && *npTimeout )
    {
        TReplyMsg.Format((char *)"Simax Socket Send TIMEOUT");
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "Simax Socket Send TIMEOUT");
        if( sMsg ) delete[] sMsg; sMsg = NULL;
        delete m_pTSocket;
        return FALSE;
    }

    if( nRet <= 0 )
    {
        TReplyMsg.Format((char *)"Simax Socket Send ERROR");
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "Simax Socket Send Error : %s", strerror(errno));
        if( sMsg ) delete[] sMsg; sMsg = NULL;
        delete m_pTSocket;
        return FALSE;
    }else
    {
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_MOS, eTAB1, "%s", TMsg.GetPrintString());
    }

    if( sMsg ) delete[] sMsg; sMsg = NULL;
    delete m_pTSocket;
    return TRUE;
}

bool CWorkflow::MES_Comm(CString TCmd, CString TData, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= TRUE*/)
{
    return MES_Comm(TCmd, TData, TRetMsg, nWaitReply);
}

bool CWorkflow::DRT_Comm(CString TCmd, CString TData, CString &TRetMsg, int nWaitReply /*= TRUE*/)
{
    // DRT_Comm는 RV가 아니라 Kafka를 통해 메시지를 전송합니다.
    // GetTrackingMsg를 사용하여 DRT용 메시지를 생성하고, CKafka를 통해 Kafka 토픽으로 보냅니다.
    CString TSendMsg;
    CString TErrMsg;
    CString TTargetSystem;

    if( GetTrackingMsg(TCmd, TData, TSendMsg, TErrMsg, TTargetSystem) == FALSE )
    {
        if ( TErrMsg.Length() > 0 )
        {
            TRetMsg.Format((char *)"Create TrackingMsg(%s) Error : %s", TCmd.GetPrintString(), TErrMsg.GetPrintString());
        }
        else
        {
            TRetMsg.Format((char *)"Create TrackingMsg Error : NOT SUPPORT CMD(%s) :: CHECK WORKFLOW", TCmd.GetPrintString());
        }

        return FALSE;
    }

    // DRT 전용 Kafka 설정을 읽습니다.
    CString TBroker = m_pTEnv->GetString("DRT", "BROKER", (char *)"localhost:9092");
    CString TTopic  = m_pTEnv->GetString("DRT", "TOPIC",  (char *)"drt-topic");
    CString TGroup  = m_pTEnv->GetString("DRT", "GROUPID", (char *)"drt-group");

    if( m_pDRT == NULL )
    {
        m_pDRT = new CKafka();

        if( m_pDRT->Init(TBroker.GetPrintString(), TTopic.GetPrintString(), TGroup.GetPrintString()) == false )
        {
            TRetMsg.Format((char *)"DRT Kafka Init Fail : %s", TBroker.GetPrintString());
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "DRT_Comm Kafka init failed for broker=%s topic=%s", TBroker.GetPrintString(), TTopic.GetPrintString());
            return FALSE;
        }
    }

    // Kafka로 메시지 전송.
    bool bResult = m_pDRT->SendMessage(TTopic.GetPrintString(), TSendMsg.GetPrintString());

    if( bResult == false )
    {
        TRetMsg.Format((char *)"DRT SEND FAIL : %s", TSendMsg.GetPrintString());
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "DRT_Comm SEND FAIL: %s", TSendMsg.GetPrintString());
        return FALSE;
    }

    // 현재 Kafka 전송은 응답 대기를 지원하지 않으므로 결과만 반환합니다.
    if( nWaitReply == TRUE )
    {
        TRetMsg.Format((char *)"DRT SEND OK");
    }
    else
    {
        TRetMsg.Format((char *)"DRT SEND QUEUED");
    }

    m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_NONE, eTAB1, "DRT_Comm SEND: %s", TSendMsg.GetPrintString());
    return TRUE;
}

bool CWorkflow::MES_Comm(CString TCmd, CString TData, CString &TRetMsg, int nWaitReply /*= TRUE*/)
{
    CString TSendMsg;
    CString TErrMsg;
    CString TTargetSystem;
    CString TBuf, TRet;
    CString TStatus, TLotNo, TPartNo, TLotQty, TLotType, TStepSeq; 
    CFilter TMOSFilter;

    TSendMsg        = "";
    TErrMsg         = "";
    TTargetSystem   = "";

    if( GetTrackingMsg(TCmd, TData, TSendMsg, TErrMsg, TTargetSystem) == FALSE )
    {
        if ( TErrMsg.Length() > 0 )
        {
            TRetMsg.Format((char *)"Create TrackingMsg(%s) Error : %s", TCmd.GetPrintString(), TErrMsg.GetPrintString());
        }else
        {
            TRetMsg.Format((char *)"Create TrackingMsg Error : NOT SUPPORT CMD(%s) :: CHECK WORKFLOW", TCmd.GetPrintString());
        }
        
        return FALSE;
    }
    
    if ( TTargetSystem == "WORKMAN" )
    {
        if( WORKMAN_Send(TSendMsg, TRetMsg, nWaitReply) == FALSE )
        {
            
            return FALSE;
        }
    }else if ( TTargetSystem == "SIMAX_OI" )
    {
        CString TTarIp;
        CString TTarPort;

        CFilter TFilter(TData.GetPrintString());

        if ( TCmd == "LABEL_PRINT" )
        {
            TTarIp   = TFilter.GetArg((char *)"CLIENTIP");
            TTarPort = TFilter.GetArg((char *)"CLIENTPORT");
        }else
        {
            TRetMsg.Format((char *)"CMD=%s_REP RESULT=FAIL STATUS=FAIL ERRORMSG=\"%s\"", TCmd.GetPrintString(), "Invaid Command");
            return FALSE;
        }

        if( MES_Comm_OiSocket(TTarIp, TTarPort, TSendMsg, TErrMsg) == FALSE )
        {
            TRetMsg.Format((char *)"CMD=%s_REP RESULT=FAIL STATUS=FAIL ERRORMSG=\"%s\"", TCmd.GetPrintString(), TErrMsg.GetPrintString());
            return FALSE;
        }else
        {
            TRetMsg.Format((char *)"CMD=%s_REP RESULT=PASS STATUS=PASS CLIENTIP=%s CLIENTPORT=%s", TCmd.GetPrintString(), TTarIp.GetPrintString(), TTarPort.GetPrintString());
        }
    }else
    {
        if( MES_Send(TSendMsg, TRetMsg, nWaitReply) == FALSE )
        {
            return FALSE;
        }
    }

    if( strstr( TRetMsg.GetPrintString(), " MSG=\"IMS_MSG=<" ) != NULL )
    {
        TMOSFilter.Reload(TRetMsg.GetPrintString());
        TBuf = TMOSFilter.GetArg((char *)"MSG");
        TBuf.Replace( '<', ' ' );
        TBuf.Replace( '>', ' ' );
        TRetMsg += " ";
        TRetMsg += TBuf;
    }

    if( strstr( TRetMsg.GetPrintString(), " MSG=RULE_MSG=<" ) != NULL )
    {
        TRetMsg.ReplaceStr(" MSG=RULE_MSG=<", " MSG=<");
        TRetMsg.Replace( '<', ' ' );
        TRetMsg.Replace( '>', ' ' );
    }

    if( TCmd == "TKIN" || TCmd == "READY_TO_WORK" || TCmd == "IMS_TKIN") 
    {
        CFilter TFilter(TData.GetPrintString());
        TMOSFilter.Reload(TRetMsg.GetPrintString());

        TStatus     = GetArgfromMES(TMOSFilter, (char *)"STATUS" );

        if ( TStatus == "PASS")
        {
            TLotNo      = GetArgfromMES(TMOSFilter, (char *)"LOTID" );
            if ( TLotNo.Length() <= 0 ) 
            { 
                TLotNo = TFilter.GetArg((char *)"LOTID"); 
            }

            TPartNo     = GetArgfromMES(TMOSFilter, (char *)"PART" );
            if ( TPartNo.Length() <= 0 ) 
            { 
                TPartNo = GetArgfromMES(TMOSFilter, (char *)"PARTID");
            }
            if ( TPartNo.Length() <= 0 ) 
            { 
                TPartNo = TFilter.GetArg((char *)"PARTNO"); 
            }

            TLotQty     = GetArgfromMES(TMOSFilter, (char *)"LOTSIZE" );
            if ( TLotQty.Length() <= 0 ) 
            { 
                TLotQty = GetArgfromMES(TMOSFilter, (char *)"QTY");
            }

            TLotType    = GetArgfromMES(TMOSFilter, (char *)"LOTTYPE" );

            TStepSeq    = GetArgfromMES(TMOSFilter, (char *)"STEPSEQ" );
            if ( TStepSeq.Length() <= 0 ) 
            { 
                TStepSeq = GetArgfromMES(TMOSFilter, (char *)"STEPID");
            }
            
            TRet = "";
            TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s WORKLOCID= UPDATE_TYPE=%s "
                                "LOTID=%s PARTNO=%s LOTTYPE=%s LOTQTY=%s STEPSEQ=%s", 
                    TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQPID"), "Update_TRACKIN", 
                    TLotNo.GetPrintString(), TPartNo.GetPrintString(), TLotType.GetPrintString(), TLotQty.GetPrintString(), TStepSeq.GetPrintString() );


            if( Set_DB_TC_EQP_STATUS(TBuf, TRet) == FALSE )
            {
            }
        }
    }
    if( TCmd == "TKOUT" || TCmd == "WORK_COMP" || TCmd == "IMS_MODULETKOUT") 
    {
        CFilter TFilter(TData.GetPrintString());
        TMOSFilter.Reload(TRetMsg.GetPrintString());

        TStatus     = GetArgfromMES(TMOSFilter, (char *)"STATUS" );

        if ( TStatus == "PASS")
        {
            TLotNo      = GetArgfromMES(TMOSFilter, (char *)"LOTID" );
            if ( TLotNo.Length() <= 0 ) 
            { 
                TLotNo = TFilter.GetArg((char *)"LOTID"); 
            }

            TRet = "";
            TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s WORKLOCID= UPDATE_TYPE=%s "
                                "LOTID=%s ", 
                    TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQPID"), "Update_TRACKOUT", 
                    TLotNo.GetPrintString() );

            if( Set_DB_TC_EQP_STATUS(TBuf, TRet) == FALSE )
            {
            }
        }
    }

    return TRUE;
}

bool CWorkflow::MES_Send(CString &TMsg, CString &TReplyMsg, int nWaitReply /*= TRUE*/)
{
    CFilter TFilter(TMsg.GetPrintString());
    CString TRvService, TRvNetwork, TRvDaemon;    // 2020-09-21

    if ( m_pTMOS_Rv == NULL )
    {
        TRvService = m_pTEnv->GetString("RV_CONF", "MES_SERVICE", DEFAULT_RV_MES_SERVICE);
        TRvNetwork = m_pTEnv->GetString("RV_CONF", "MES_NETWORK", DEFAULT_RV_MES_NETWORK);
        TRvDaemon  = m_pTEnv->GetString("RV_CONF", "MES_DAEMON" , DEFAULT_RV_MES_DAEMON );

        // MOS I/F RV
        m_pTMOS_Rv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

        if( m_pTMOS_Rv->Rv_Init( m_TRvMosSubject, m_TRvMosSubject ) == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "RV INIT ERROR");
        }
    }
    // 2020-09-21 END

    int nRet = m_pTMOS_Rv->SendRvMsg ( m_pTEnv->GetString("RV_SUBJECT", "MES", DEFAULT_RV_MES_SUBJECT), TMsg );

    if( nRet == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_MOS, eTAB1, "SEND : %s : TO=%s", TMsg.GetPrintString(), m_pTEnv->GetString("RV_SUBJECT", "MES", DEFAULT_RV_MES_SUBJECT));
    }
    else
    {
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_MOS, eTAB1, "%s : TO=%s", TMsg.GetPrintString(), m_pTEnv->GetString("RV_SUBJECT", "MES", DEFAULT_RV_MES_SUBJECT));
        m_TQALog.WriteQA("[%s][MOS Send][%s]", TFilter.GetArg((char *)"EQPID"), TMsg.GetPrintString());
    }

    if( nRet == FALSE )
    {
        TReplyMsg.Format((char *)"MES REPLY SEND NG. %s", strerror(errno));
        return FALSE;
    }

    if( nWaitReply == TRUE )
    {
        CString TRvMsg;
        int     nRvMsgType;

        nRet = m_pTMOS_Rv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );

        if( nRet == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_MOS, eTAB1, "RECV : %s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_MOS, eTAB1, "%s", TRvMsg.GetPrintString());
            m_TQALog.WriteQA("[%s][MOS Recv][%s]", TFilter.GetArg((char *)"EQPID"), TRvMsg.GetPrintString());
        }

        if( nRet == FALSE )
        {
            TReplyMsg = "MES REPLY TIMEOUT";
            return FALSE;
        }

        TReplyMsg = TRvMsg;
    }
    return TRUE;
}

bool CWorkflow::WORKMAN_Send(CString &TMsg, CString &TReplyMsg, int nWaitReply /*= TRUE*/)
{
    CFilter TFilter(TMsg.GetPrintString());
    CString TRvService, TRvNetwork, TRvDaemon;    // 2020-09-21

    if ( m_pTMOS_Rv == NULL )
    {
        TRvService = m_pTEnv->GetString("RV_CONF", "MES_SERVICE", DEFAULT_RV_MES_SERVICE);
        TRvNetwork = m_pTEnv->GetString("RV_CONF", "MES_NETWORK", DEFAULT_RV_MES_NETWORK);
        TRvDaemon  = m_pTEnv->GetString("RV_CONF", "MES_DAEMON" , DEFAULT_RV_MES_DAEMON );

        // MOS I/F RV
        m_pTMOS_Rv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

        if( m_pTMOS_Rv->Rv_Init( m_TRvMosSubject, m_TRvMosSubject ) == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "RV INIT ERROR");
        }
    }
    // 2020-09-21 END

    int nRet = m_pTMOS_Rv->SendRvMsg ( m_pTEnv->GetString("RV_SUBJECT", "WORKMAN", DEFAULT_RV_WORKMAN_SUBJECT), TMsg );

    if( nRet == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_WORKMAN, eTAB1, "SEND : %s : TO=%s", TMsg.GetPrintString(), m_pTEnv->GetString("RV_SUBJECT", "WORKMAN", DEFAULT_RV_WORKMAN_SUBJECT));
    }
    else
    {
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_WORKMAN, eTAB1, "%s : TO=%s", TMsg.GetPrintString(), m_pTEnv->GetString("RV_SUBJECT", "WORKMAN", DEFAULT_RV_WORKMAN_SUBJECT));
        m_TQALog.WriteQA("[%s][WORKMAN Send][%s]", TFilter.GetArg((char *)"EQPID"), TMsg.GetPrintString());
    }

    if( nRet == FALSE )
    {
        TReplyMsg.Format((char *)"WORKMAN REPLY SEND NG. %s", strerror(errno));
        return FALSE;
    }

    if( nWaitReply == TRUE )
    {
        CString TRvMsg;
        int     nRvMsgType;

        nRet = m_pTMOS_Rv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );
        
        if( nRet == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_WORKMAN, eTAB1, "RECV : %s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_WORKMAN, eTAB1, "%s", TRvMsg.GetPrintString());
            m_TQALog.WriteQA("[%s][WORKMAN Recv][%s]", TFilter.GetArg((char *)"EQPID"), TRvMsg.GetPrintString());
        }

        if( nRet == FALSE )
        {
            TReplyMsg = "WORKMAN REPLY TIMEOUT";
            return FALSE;
        }

        TReplyMsg = TRvMsg;
    }
    return TRUE;
}

bool CWorkflow::RV_SendLocal(CString TTargetSubject, CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= 0*/)
{
    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    ptr = strstr(TMsg.GetPrintString(), " EQP_INFO=(");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    CString TBuf;

    if( nWaitReply == 1 )
    {
        ptr = strstr(TMsg.GetPrintString(), " REPLY=");
        if( !ptr )
        {
            TBuf.Format((char *)" REPLY=Y");
            TMsg += TBuf;
        }
    }

    ptr = strstr(TMsg.GetPrintString(), " MSG_SEQ=");
    if( !ptr )
    {
        TBuf.Format((char *)" MSG_SEQ=%s", m_TMsgSeq.GetPrintString());
        TMsg += TBuf;
    }

    TBuf.Format((char *)" FROM=%s", m_TRvSubject.GetPrintString());
    TMsg += TBuf;

    TBuf.Format((char *)" TO=%s", TTargetSubject.GetPrintString());
    TMsg += TBuf;


    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    int nRet = m_pTRv->SendRvMsg ( TTargetSubject, TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_NONE, eTAB1, "%s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);
    }
    else
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);
    }

    if( nWaitReply == 1 )
    {
        CString TRvMsg;
        int     nRvMsgType;

        m_TBinData     = "";
        m_TBodyData    = "";
        m_TDmsData     = "";

        nRet = m_pTRv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );

        if( nRet == TRUE )
        {
            m_TBinData     = m_pTRv->GetBinData( MESSAGE_BINARY );
            m_TBodyData    = m_pTRv->GetBinData( MESSAGE_PPBODY );
            m_TDmsData     = m_pTRv->GetBinData( MESSAGE_DMS    );
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_NONE, eTAB1, "%s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "RECV NG");
        }

        if( nRet == FALSE )
        {
            TRetMsg.Format((char *) "SEND TO %s NG. TIMEOUT(%s)", TTargetSubject.GetPrintString(), m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT));
            return FALSE;
        }

        TRetMsg = TRvMsg;
    }
    return TRUE;
}

bool CWorkflow::ANOMALY_Send(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply /*= 0*/)
{
    CRvMain*    pTRv;
    CString TRvService, TRvNetwork, TRvDaemon;
    RV_CAST eRvType = RV_MULTICAST;

    char *ptr = strstr(TMsg.GetPrintString(), " FROM=");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    ptr = strstr(TMsg.GetPrintString(), " EQP_INFO=(");
    if( ptr )
    {
        memset(ptr, ' ', strlen(ptr));
        TMsg.Trim();
    }

    CString TBuf;

    TBuf.Format((char *)" FROM=%s", m_TEqpRvSubject.GetPrintString());
    TMsg += TBuf;

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    TRvService = m_pTEnv->GetString("RV_CONF", "SERVICE", DEFAULT_RV_SERVICE);
    TRvNetwork = m_pTEnv->GetString("RV_CONF", "NETWORK", DEFAULT_RV_NETWORK);
    TRvDaemon  = m_pTEnv->GetString("RV_CONF", "DAEMON" , DEFAULT_RV_DAEMON);

    pTRv = new CRvMain(m_pTLog, RV_SENDER_ON, RV_LISTENER_ON, RV_MSG_BLOCK, RV_MULTICAST, TRvService.GetPrintString(), TRvNetwork.GetPrintString(), TRvDaemon.GetPrintString() );

    TBuf.Format((char *)"%s.TMP%d", m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT), getpid());
    if( pTRv->Rv_Init( TBuf, TBuf ) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "EQP SEND RV INIT");

        if( pTRv ) delete pTRv; pTRv = NULL;
        return FALSE;
    }

    int nRet = pTRv->SendRvMsg ( m_pTEnv->GetString("RV_SUBJECT", "ANOMALY", DEFAULT_RV_ANOMALY_SUBJECT), TMsg, nRvType, TBinMsg, TPPBodyMsg, TDmsMsg );

    if( nRet == TRUE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_SEND, eLOG_IFS_EQP, eTAB1, "%s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);
    }
    else
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, eTAB1, "SEND NG : %s : TYPE=[%d]", TMsg.GetPrintString(), nRvType);
    }

    if( pTRv ) delete pTRv; pTRv = NULL;

    if( nWaitReply == 1 )
    {
        CString TRvMsg;
        int     nRvMsgType;

        m_TBinData     = "";
        m_TBodyData    = "";
        m_TDmsData     = "";

        nRet = m_pTEqpRv->GetRvMessage( TRvMsg, nRvMsgType, atoi(m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT)) );

        if( nRet == TRUE )
        {
            m_TBinData     = m_pTEqpRv->GetBinData( MESSAGE_BINARY );
            m_TBodyData    = m_pTEqpRv->GetBinData( MESSAGE_PPBODY );
            m_TDmsData     = m_pTEqpRv->GetBinData( MESSAGE_DMS    );
            m_pTLog->WriteTC(eLOG_TYPE_RECV, eLOG_IFS_EQP, eTAB1, "%s", TRvMsg.GetPrintString());
        }
        else
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, eTAB1, "RECV NG : TIMEOUT=%s", m_pTEnv->GetString("RV_CONF", "TIMEOUT", DEFAULT_RV_TIMEOUT));
        }

        TRetMsg = TRvMsg;
    }

    return TRUE;
}

bool CWorkflow::TC_S1F14(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    CString TBuf, TEventLinkInfo, TEqpStatusInfo;
    CFilter TFilter(TMsg.GetPrintString());

    if( CheckPara(TFilter, "PROCESS"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "%s", TBuf.GetPrintString()); return FALSE; }
    if( CheckPara(TFilter, "LINE"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "%s", TBuf.GetPrintString()); return FALSE; }
    if( CheckPara(TFilter, "EQP_MODEL" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "%s", TBuf.GetPrintString()); return FALSE; }
    if( CheckPara(TFilter, "EQPID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "%s", TBuf.GetPrintString()); return FALSE; }

    if( m_TOra.ConnectDB() == FALSE ) 
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "DB Connect ERROR.");
        return FALSE;
    }

    // Event Link Data
    TEventLinkInfo = "";

    if( m_TOra.GetEventLink(TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"), TEventLinkInfo) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "Get HSMS Event Link NG. PROCESS(%s) LINE(%s) EQP_MODEL(%s)",
                    TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"));
        return FALSE;
    }

    // Eqp Status Data
    TBuf.Format((char *)"PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s WORKLOCID=%s SELECT_TYPE=%s",
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"), TFilter.GetArg((char *)"EQPID"), 
            "LD", "SELECT_EQP" );
    TEqpStatusInfo = "";

    if( m_TOra.Get_EQP_STATUS(TBuf, TEqpStatusInfo) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "Get HSMS Event Link NG. PROCESS(%s) LINE(%s) EQP_MODEL(%s)",
                    TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"));
        return FALSE;
    }

    TReplyMsg.Format((char *)"CMD=EVENT_LINK PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s %s %s",
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"), TFilter.GetArg((char *)"EQPID"), TEventLinkInfo.GetPrintString(), TEqpStatusInfo.GetPrintString());
    return TRUE;
}

bool CWorkflow::TC_S5F1(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    CString TRet;
    CString TSubject, TSendMsg;
    CFilter TFilter(TMsg.GetPrintString());

    TSendMsg.Format( "TOOL_ALARM HDR=(%s,%s,%s) TIME_STAMP=%s EQPID=%s ALCD=%s ALID=%s ALARM_MESSAGE=\"%s\" ALARM_TYPE=%s LOTID=%s PARTNO=%s LOTTYPE=%s ", 
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(), 
                TFilter.GetArg((char *)"EQPID"), 
                ( TFilter.GetArg((char *)"ALCD")          == NULL ) ? "" : TFilter.GetArg((char *)"ALCD"),
                ( TFilter.GetArg((char *)"ALID")          == NULL ) ? "" : TFilter.GetArg((char *)"ALID"),
                ( TFilter.GetArg((char *)"ALTX")          == NULL ) ? "" : TFilter.GetArg((char *)"ALTX"),
                ( TFilter.GetArg((char *)"ALARM_TYPE")    == NULL ) ? "" : TFilter.GetArg((char *)"ALARM_TYPE"),
                ( TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID" )   == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID" ),
                ( TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO" )  == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO" ),
                ( TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTTYPE" ) == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTTYPE" ) );

    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::TC_S14F1(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TRet, TErrMsg, TEqpSubject;
    CString TTrackingMsg;
    CFilter TMsgFilter;

    MAPINFO MapData ;

    int bFlowFlag = TRUE;

    //CMD=S14F1 SYSTEMBYTE=3 PROCESS=COW LINE=CAS1 EQP_MODEL=FC5000 EQPID=COW01 FRAMEID=D58303P31H2710063 ORIGIN=UpperLeft MAPKIND=StripMap  
    //CMD=S14F1 SYSTEMBYTE=3 PROCESS=COW LINE=CAS1 EQP_MODEL=FC5000 EQPID=COW01 FRAMEID=D58303P31H2710063 ORIGIN=UpperLeft MAPKIND=StripMap 
    //  OBJSPEC=\"%s\" 2D_ATTR1=\"%s\" 2D_ATTR1RELN=\"%s\" " "ATTRID1=\"%s\" ATTRID2=\"%s\" ATTRID3=\"%s\" ATTRID4=\"%s\" ATTRID5=\"%s\" ATTRID6=\"%s\" ATTRID7=\"%s\" ATTRID8=\"%s\" ATTRID9=\"%s\" "

    CString TLotID, TArrayX, TArrayY, TEqpChipIdInfo;
    CString TEqpModel   = TFilter.GetArg((char *)"EQP_MODEL"    );
    CString TSystemByte = TFilter.GetArg((char *)"SYSTEMBYTE"   );
    CString TFrameId    = TFilter.GetArg((char *)"FRAMEID"      );
    CString TOrigin     = TFilter.GetArg((char *)"ORIGIN"       );
    CString TMapKind    = TFilter.GetArg((char *)"MAPKIND"      );
    TOrigin.Upper();

    if ( TFrameId == "" || TFrameId == NULL )
    {
        bFlowFlag = FALSE;
        TErrMsg.Format((char*)"NOT EXIST FRAMEID. CHECK FRAMEID READ FUNCTION!!");
    }

    if( bFlowFlag == TRUE )
    {
        //CMD=FRAMEDISPLAY_REP RESULT=PASS PROCESS=CHIPMNT LINE=CAS1 EQP_ID=ACB03A STATUS=PASS LOTID="GED0310D" ARRAY_X="30" ARRAY_Y="6" 
        // FRAME_MAP_INFO="001010101010101010101010101010010101010101010101010101010100001010101010101010101010101010010101010101010101010101010100001010101010101010101010101010010101010101010101010101010100" 
        // MAINQTY="249" STEPSEQ="P625" PARTNUMBER="K4A8GE45WB-G7D4YT" LOTSTATE="WAIT" 
        // MCP_SEQ="001010101010101010101010101010010101010101010101010101010100001010101010101010101010101010010101010101010101010101010100001010101010101010101010101010010101010101010101010101010100" MAPPING_STEPSEQ="P577"

        //CMD=FRAMEDISPLAY_REP RESULT=PASS PROCESS=CHIPMNT LINE=CAS1 EQP_ID=ACB20 STATUS=FAIL ERRORMSG="Mapping is wrong!"

        TTrackingMsg.Format((char *)"PROCESS=%s LINE=%s EQPID=%s FRAMEID=%s STEPDESC=MOLD ", 
                TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQPID"),
                TFrameId.GetPrintString() );

        if ( TMapKind == "BoatMap" )
        {
            TTrackingMsg += "CHIPIDINFO=Y ";
        }

        if( MES_Comm("FRAMEDISPLAY", TTrackingMsg, TRet) == FALSE )
        {
            bFlowFlag = FALSE;
            TErrMsg = TRet;
        }else
        {
            TMsgFilter.Reload(TRet.GetPrintString());

            if( strcmp(TMsgFilter.GetArg((char *)"STATUS"), "PASS") != 0 )
            {
                bFlowFlag = FALSE;
                TErrMsg.Format("%s", TMsgFilter.GetArg((char *)"ERRORMSG") );
            }
        }
    }

    if( bFlowFlag == TRUE )
    {
        TLotID              = GetArgfromMES(TMsgFilter, (char *)"LOTID");
        TArrayX             = GetArgfromMES(TMsgFilter, (char *)"ARRAY_X");
        TArrayY             = GetArgfromMES(TMsgFilter, (char *)"ARRAY_Y");
        MapData.nTotalCol   = atoi(TArrayX.GetPrintString());
        MapData.nTotalRow   = atoi(TArrayY.GetPrintString());
        MapData.TMapData    = GetArgfromMES(TMsgFilter, (char *)"FRAME_MAP_INFO");
        MapData.TMCPSeq     = GetArgfromMES(TMsgFilter, (char *)"MCP_SEQ");
        MapData.TWaferBin   = GetArgfromMES(TMsgFilter, (char *)"WAFERBIN");
        MapData.TChipIDInfo = GetArgfromMES(TMsgFilter, (char *)"CHIPIDINFO");

       

        if ( TOrigin != "UPPERLEFT" )
        {
            Rotaion_FrameMap(TOrigin, &MapData );

            if ( MapData.TChipIDInfo.Length() > 0 )
            {
                Rotaion_ChipInfo(TOrigin, atoi(TArrayX.GetPrintString()), atoi(TArrayY.GetPrintString()), MapData.TChipIDInfo, TEqpChipIdInfo, TBuf);  
                m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " TC_S14F1() :: MapData.TChipIDInfo=[%s]", MapData.TChipIDInfo.GetPrintString());
            }
        }

        if (      TOrigin == "UPPERLEFT"  ) { TOrigin.Format((char *)"2"); }
        else if ( TOrigin == "UPPERRIGHT" ) { TOrigin.Format((char *)"1"); }
        else if ( TOrigin == "LOWERLEFT"  ) { TOrigin.Format((char *)"3"); }
        else if ( TOrigin == "LOWERRIGHT" ) { TOrigin.Format((char *)"4"); }
    }

    // CMD=GETATTR PROCESS=%s LINE=%s EQPID=%s SYSTEMBYTE=123
    //      OBJID="E64986PA1H0310001" OBJACK="PASS/FAIL" REPLY=N ATTRLIST=4 ATTRID1="OriginLocation" ATTRDATA1=\"1\" ATTRTYPE1=U1 ATTRID2="Rows" ATTRDATA2=\"8\" ATTRTYPE2=U4 
    //      ATTRID3="Columns" ATTRDATA3=\"16\" ATTRTYPE3=U4 ATTRID4="CellGrade" ATTRDATA4=\"REF_MAP_INFO/REF_MAP_INFO_SEQ/REF_MAP_INFO_WAFERBIN\" ATTRTYPE4=B ATTRID5="DeviceID" ATTRDATA5=\"PkgID1,PkgID2,...\" ATTRTYPE4=A
    //      MAP_INFO=\"0000000000010001000000000001000100\" MAP_INFO_SEQ=\"0000000000010001000000000001000100\" MAP_INFO_WAFERBIN=\"0000000000010001000000000001000100\" MAP_DATA_TYPE=\"ASCII/BINARY/REVERSE_Y/REVERSE_N\"

    CObjAttrMsg TObjAttr("GETATTR", "-", "-", TFrameId, RV_REPLY_OFF);

    if ( bFlowFlag == TRUE )
    {
        TObjAttr.SetObjAck      ("PASS");
        TObjAttr.SetMapInfo     (MapData.TMapData);
        TObjAttr.SetMapInfoSeq  (MapData.TMCPSeq);
        TObjAttr.SetMapInfoWFBin(MapData.TWaferBin);
        TObjAttr.SetSystemByte  (TSystemByte);
        TObjAttr.SetMapDataType ("ASCII");
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID1"),     TOrigin,            "U1");      // OriginLocation
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID2"),     TArrayY,            "U4");      // Rows    : (ARRAY_Y)
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID3"),     TArrayX,            "U4");      // Columns : (ARRAY_X)
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID4"),     "REF_MAP_INFO",     "B");       // CellGrade
        
        if ( TMapKind == "BoatMap" )
        {
            TObjAttr.Add(TFilter.GetArg((char *)"ATTRID5"),     TEqpChipIdInfo,     "A");       // CHIPID
        }

    }else
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " TC_S14F1() :: ErrMsg=[%s]", TErrMsg.GetPrintString());

        TObjAttr.SetObjAck      ("FAIL");
        TObjAttr.SetMapInfo     ("");
        TObjAttr.SetMapInfoSeq  ("");
        TObjAttr.SetMapInfoWFBin("");
        TObjAttr.SetSystemByte  (TSystemByte);
        TObjAttr.SetMapDataType ("");
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID1"),     TOrigin,            "U1");      // OriginLocation
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID2"),     "0",                "U4");      // Rows    : (ARRAY_Y)
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID3"),     "0",                "U4");      // Columns : (ARRAY_X)
        TObjAttr.Add(TFilter.GetArg((char *)"ATTRID4"),     "",                 "B");       // CellGrade
        
        if ( TMapKind == "BoatMap" )
        {
            TObjAttr.Add(TFilter.GetArg((char *)"ATTRID5"),     "",     "A");       // CHIPID
        }

    }

    TBuf.Format((char *)"CMD=%s PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s %s",
            TObjAttr.GetCmd().GetPrintString(), 
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"), TFilter.GetArg((char *)"EQPID"), 
            TObjAttr.GetPrintString().GetPrintString());

    TEqpSubject = GetEqpSubject(TFilter.GetArg((char *)"EQPID"));

    if( EQP_Send( TEqpSubject, TBuf, TBinMsg, TPPBodyMsg, TDmsMsg, TRet, RV_REPLY_OFF) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, "OBJATTR MSG NG");
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::TC_CommStatus(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=COMM_STATUS PROCESS=BACKLAP LINE=YAS1 EQP_MODEL=PG300RM EQPID=BL01 STATUS=OFF EVENT_INIT=YES TDS=YES/NO EES=YES/NO

    CFilter TFilter(TMsg.GetPrintString());
    CString TSendTDS, TSendEES, TStatus, TEventInit, TSendMsg;
    CString TSubject;
    CString TTimeStamp = TFilter.GetArg((char *)"TIME_STAMP");    if ( TTimeStamp.Length() == 0 ) { TTimeStamp = GetDateTime_milisec(); }

    TStatus     = TFilter.GetArg((char *)"STATUS");
    if( TStatus.Length() <= 0 )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_FDC, eTAB1, "CMD=%s : NOT FOUND STATUS", TFilter.GetArg((char *)"CMD"));
        return FALSE;
    }

    TSendTDS    = TFilter.GetArg((char *)"TDS");
    TSendEES    = TFilter.GetArg((char *)"EES");
    TEventInit  = TFilter.GetArg((char *)"EVENT_INIT");

    CString TRet;

    if( TSendTDS == "YES" )
    {
        TSubject = GetFDCSubject();
        if( TStatus == "ON" )
        {
            TSendMsg.Format( "CMD=TOOLCOMM TIME_STAMP=%s EQPID=%s STATUS=ON", 
                    TTimeStamp.GetPrintString(), TFilter.GetArg((char *)"EQPID"));

            FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet);

            if( TEventInit == "YES" ) 
            {
                TSendMsg.Format( "CMD=GETSENSORS TIME_STAMP=%s EQPID=%s", 
                            TTimeStamp.GetPrintString(), TFilter.GetArg((char *)"EQPID") );

                FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet);
            }
        }
        else
        {
            TSendMsg.Format( "CMD=TOOLCOMM TIME_STAMP=%s EQPID=%s STATUS=OFF", 
                        TTimeStamp.GetPrintString(), TFilter.GetArg((char *)"EQPID"));

            FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet);
        }
    }

    if( TStatus == "ON" )
    {
        TSendMsg.Format( "OFFLINE_END HDR=(%s,%s,%s) TIME_STAMP=%s EQPID=%s CEID=%s LOTID=%s PPID=%s PARTID=%s ", 
                    m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                    m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                    TFilter.GetArg((char *)"EQPID"),
                    TTimeStamp.GetPrintString(), 
                    TFilter.GetArg((char *)"EQPID"), 
                    "OFFLINE_END",
                    ( TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID")    == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID"),
                    ( TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID") == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID"),
                    ( TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO")   == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO"));


        EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet);
    }
    else
    {
        TSendMsg.Format( "OFFLINE_START HDR=(%s,%s,%s) TIME_STAMP=%s EQPID=%s CEID=%s LOTID=%s PPID=%s PARTID=%s ", 
                    m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                    m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                    TFilter.GetArg((char *)"EQPID"),
                    TTimeStamp.GetPrintString(), 
                    TFilter.GetArg((char *)"EQPID"), 
                    "OFFLINE_START",
                    ( TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID")    == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID"),
                    ( TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID") == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID"),
                    ( TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO")   == NULL ) ? "" : TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO"));

        EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet);
    }

    return TRUE;
}

bool CWorkflow::TC_ToolData(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, CString TSensorValue)
{
    // MSG FORMAT
    //      CMD=TOOLDATA TIME_STAMP=%s EQPID=%s LOTID=(%s) RECIPEID=(%s) PARTNO=(%s) ESPECID=(%s) SENSOR_VALUES={%s}
    CString TRet;
    CString TSubject, TSendMsg;
    CFilter TFilter(TMsg.GetPrintString());

    CString TTimeStamp  = TFilter.GetArg((char *)"TIME_STAMP");    if ( TTimeStamp.Length() == 0 ) { TTimeStamp = GetDateTime_milisec(); }

    if ( TSensorValue.Length() == 0 )
    {
        TSensorValue = TFilter.GetArg((char *)"SENSOR_VALUES");
    }

    TSubject = GetFDCSubject();

    TSendMsg.Format( "CMD=TOOLDATA TIME_STAMP=%s EQPID=%s LOTID=(%s) RECPID=(%s) PARTNO=(%s) LOTTYPE=(%s) ESPECID=(%s) STEP=(%s) SENSOR_VALUES={%s} ", 
                TTimeStamp.GetPrintString(), 
                TFilter.GetArg((char *)"EQPID"), 
                ( TFilter.GetArg((char *)"LOTID")           == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID")    : TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"RECIPEID")        == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID") : TFilter.GetArg((char *)"RECIPEID"),
                ( TFilter.GetArg((char *)"PARTNO")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO")   : TFilter.GetArg((char *)"PARTNO"),
                ( TFilter.GetArg((char *)"LOTTYPE")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTTYPE")  : TFilter.GetArg((char *)"LOTTYPE"),
                ( TFilter.GetArg((char *)"ESPECID")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"ESPECID")  : TFilter.GetArg((char *)"ESPECID"),
                ( TFilter.GetArg((char *)"STEPID")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"STEPSEQ")  : TFilter.GetArg((char *)"STEPID"),
                TSensorValue.GetPrintString() );

    if( FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::TC_ToolRecipe(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, CString TSensorValue)
{
    // MSG FORMAT
    //      CMD=TOOLDATA TIME_STAMP=%s EQPID=%s LOTID=(%s) RECIPEID=(%s) PARTNO=(%s) ESPECID=(%s) SENSOR_VALUES={%s}
    CString TRet;
    CString TSubject, TSendMsg;
    CFilter TFilter(TMsg.GetPrintString());

    CString TTimeStamp  = TFilter.GetArg((char *)"TIME_STAMP");    if ( TTimeStamp.Length() == 0 ) { TTimeStamp = GetDateTime_milisec(); }

    if ( TSensorValue.Length() == 0 )
    {
        TSensorValue = TFilter.GetArg((char *)"SENSOR_VALUES");
    }

    TSubject = GetFDCSubject();

    TSendMsg.Format( "CMD=TOOLRECIPE TIME_STAMP=%s EQPID=%s LOTID=(%s) RECPID=(%s) PARTNO=(%s) LOTTYPE=(%s) STEP=(%s) ESPECID=(%s) TRACKNO=(%s) SENSOR_VALUES={%s} ", 
                TTimeStamp.GetPrintString(), 
                TFilter.GetArg((char *)"EQPID"), 
                ( TFilter.GetArg((char *)"LOTID")           == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID")    : TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"RECIPEID")        == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID") : TFilter.GetArg((char *)"RECIPEID"),
                ( TFilter.GetArg((char *)"PARTNO")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO")   : TFilter.GetArg((char *)"PARTNO"),
                ( TFilter.GetArg((char *)"LOTTYPE")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTTYPE")  : TFilter.GetArg((char *)"LOTTYPE"),
                ( TFilter.GetArg((char *)"STEPID")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"STEPSEQ")  : TFilter.GetArg((char *)"STEPID"),
                ( TFilter.GetArg((char *)"ESPECID")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"ESPECID")  : TFilter.GetArg((char *)"ESPECID"),
                ( TFilter.GetArg((char *)"TRACKNO")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"TRACKNO")  : TFilter.GetArg((char *)"TRACKNO"),
                TSensorValue.GetPrintString() );

    if( FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::TC_ToolEvent(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, CString TSensorValue, int nSendFDC, int nSendEES)
{
    // MSG FORMAT
    //      CMD=TOOLEVENT TIME_STAMP=%s EQPID=%s LOTID=(%s) RECIPEID=(%s) PARTNO=(%s) ESPECID=(%s) SENSOR_VALUES={%s}
    CString TRet;
    CString TSubject, TSendMsg;
    CFilter TFilter(TMsg.GetPrintString());

    CString TTimeStamp  = TFilter.GetArg((char *)"TIME_STAMP");    if ( TTimeStamp.Length() == 0 ) { TTimeStamp = GetDateTime_milisec(); }
    CString TSendFDC    = TFilter.GetArg((char *)"SEND_FDC");
    CString TSendEES    = TFilter.GetArg((char *)"SEND_EES");

    if ( TSensorValue.Length() == 0 )
    {
        TSensorValue = TFilter.GetArg((char *)"SENSOR_VALUES");
    }

    if ( nSendFDC == TRUE || TSendFDC == "TRUE" || TSendFDC == "Y" || TSendFDC == "YES" )
    {
        TSubject = GetFDCSubject();

        TSendMsg.Format( "CMD=TOOLEVENT TIME_STAMP=%s EQPID=%s CEID=%s LOTID=(%s) RECPID=(%s) PARTNO=(%s) LOTTYPE=(%s) STEP=(%s) TRACKNO=%s SENSOR_VALUES={%s} ", 
                    TTimeStamp.GetPrintString(), 
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"CEID"),
                    ( TFilter.GetArg((char *)"LOTID")           == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID")    : TFilter.GetArg((char *)"LOTID"),
                    ( TFilter.GetArg((char *)"RECIPEID")        == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID") : TFilter.GetArg((char *)"RECIPEID"),
                    ( TFilter.GetArg((char *)"PARTNO")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO")   : TFilter.GetArg((char *)"PARTNO"),
                    ( TFilter.GetArg((char *)"LOTTYPE")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTTYPE")  : TFilter.GetArg((char *)"LOTTYPE"),
                    ( TFilter.GetArg((char *)"STEPID")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"STEPSEQ")  : TFilter.GetArg((char *)"STEPID"),
                    ( TFilter.GetArg((char *)"TRACKNO")         == NULL ) ? ""                                                     : TFilter.GetArg((char *)"TRACKNO"),
                    TSensorValue.GetPrintString() );

        if( FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
        {
        }
    }

    if ( nSendEES == TRUE || TSendEES == "TRUE" || TSendEES == "Y" || TSendEES == "YES" )
    {
        TSendMsg.Format( "TOOL_EVENT HDR=(%s,%s,%s) TIME_STAMP=%s EQPID=%s CEID=%s LOTID=%s PPID=%s PARTNO=%s STEP=%s SIDE=%s LOTTYPE=%s QTY=%s VID_VALUES={%s} TRANSACTIONID=%s ", 
                    m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                    m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                    TFilter.GetArg((char *)"EQPID"),
                    TTimeStamp.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"CEID"),
                    ( TFilter.GetArg((char *)"LOTID")           == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTID")    : TFilter.GetArg((char *)"LOTID"),
                    ( TFilter.GetArg((char *)"RECIPEID")        == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"RECIPEID") : TFilter.GetArg((char *)"RECIPEID"),
                    ( TFilter.GetArg((char *)"PARTNO")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"PARTNO")   : TFilter.GetArg((char *)"PARTNO"),
                    ( TFilter.GetArg((char *)"STEPID")          == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"STEPSEQ")  : TFilter.GetArg((char *)"STEPID"),
                    ( TFilter.GetArg((char *)"SIDE")            == NULL ) ? ""                                                     : TFilter.GetArg((char *)"SIDE"),
                    ( TFilter.GetArg((char *)"LOTTYPE")         == NULL ) ? TFilter.GetArg((char *)"EQP_INFO", (char *)"LOTTYPE")  : TFilter.GetArg((char *)"LOTTYPE"),
                    ( TFilter.GetArg((char *)"QTY")             == NULL ) ? ""                                                     : TFilter.GetArg((char *)"QTY"),
                    TSensorValue.GetPrintString(),
                    TFilter.GetArg((char *)"SYSTEMBYTE") );

        if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
        {
        }
    }

    return TRUE;
}

bool CWorkflow::TC_ToolAlarm(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, int nSendFDC, int nSendEES)
{
    // MSG FORMAT
    //      CMD=TOOLALARM PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s TIME_STAMP=%s ALCD=%s ALID=%s ALARM_MESSAGE="%s"
    CString TRet;
    CString TSubject, TSendMsg;
    CFilter TFilter(TMsg.GetPrintString());

    CString TTimeStamp  = TFilter.GetArg((char *)"TIME_STAMP");    if ( TTimeStamp.Length() == 0 ) { TTimeStamp = GetDateTime_milisec(); }
    CString TSendFDC    = TFilter.GetArg((char *)"SEND_FDC");
    CString TSendEES    = TFilter.GetArg((char *)"SEND_EES");

    if ( nSendFDC == TRUE || TSendFDC == "TRUE" || TSendFDC == "Y" || TSendFDC == "YES" )
    {
        TSubject = GetFDCSubject();

        TSendMsg.Format( "CMD=TOOLALARM TIME_STAMP=%s EQPID=%s ALCD=%s ALID=%s ALARM_MESSAGE=\"%s\" ", 
                    TTimeStamp.GetPrintString(), 
                    TFilter.GetArg((char *)"EQPID"), 
                    ( TFilter.GetArg((char *)"ALCD")          == NULL ) ? "" : TFilter.GetArg((char *)"ALCD"),
                    ( TFilter.GetArg((char *)"ALID")          == NULL ) ? "" : TFilter.GetArg((char *)"ALID"),
                    ( TFilter.GetArg((char *)"ALARM_MESSAGE") == NULL ) ? "" : TFilter.GetArg((char *)"ALARM_MESSAGE") );

        if( FDC_Send(TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
        {
        }
    }

    if ( nSendEES == TRUE || TSendEES == "TRUE" || TSendEES == "Y" || TSendEES == "YES" )
    {
        TSendMsg.Format( "TOOL_ALARM HDR=(%s,%s,%s) TIME_STAMP=%s EQPID=%s ALCD=%s ALID=%s ALARM_MESSAGE=\"%s\" ALARM_TYPE=%s LOTID=%s PARTNO=%s LOTTYPE=%s QTY=%s ", 
                    m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                    m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                    TFilter.GetArg((char *)"EQPID"),
                    TTimeStamp.GetPrintString(), 
                    TFilter.GetArg((char *)"EQPID"),
                    ( TFilter.GetArg((char *)"ALCD")          == NULL ) ? "" : TFilter.GetArg((char *)"ALCD"),
                    ( TFilter.GetArg((char *)"ALID")          == NULL ) ? "" : TFilter.GetArg((char *)"ALID"),
                    ( TFilter.GetArg((char *)"ALARM_MESSAGE") == NULL ) ? "" : TFilter.GetArg((char *)"ALARM_MESSAGE"),
                    ( TFilter.GetArg((char *)"ALARM_TYPE")    == NULL ) ? "" : TFilter.GetArg((char *)"ALARM_TYPE"),
                    ( TFilter.GetArg((char *)"LOTID" )        == NULL ) ? "" : TFilter.GetArg((char *)"LOTID"),
                    ( TFilter.GetArg((char *)"PARTNO" )       == NULL ) ? "" : TFilter.GetArg((char *)"PARTNO"),
                    ( TFilter.GetArg((char *)"LOTTYPE" )      == NULL ) ? "" : TFilter.GetArg((char *)"LOTTYPE"),
                    ( TFilter.GetArg((char *)"QTY" )          == NULL ) ? "" : TFilter.GetArg((char *)"QTY"));

        if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
        {
            return FALSE;
        }
    }

    return TRUE;
}

bool CWorkflow::RMM_EqpRecipeUpload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=EQP_RECIPE_UPLOAD TIME_STAMP=2002/10/21-23:59:59.9 EQPID=WB01 
    //              ACTION_MODE=V RECIPEID= ADS12345-MS MULTI=Y DMS_RECIPEID= ADS12345-001-NA ESPECID=ESPEC01 TRANSACTIONID=12

    // RMS�� ����
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"EQP_RECIPE_UPLOAD HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s ACTION_MODE=%s "
                        "RECIPEID=\"%s\" MULTI=%s DMS_RECIPEID=%s "
                        "LOTID=%s PARTNO=%s LOTTYPE=%s STEP=%s "
                        "ESPECID=\"%s\" AUTO_GOLDEN=%s TRANSACTIONID=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),        TFilter.GetArg((char *)"ACTION_MODE"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"MULTI"),        TFilter.GetArg((char *)"DMS_RECIPEID"),
                TFilter.GetArg((char *)"LOTID"),        TFilter.GetArg((char *)"PARTNO"),       TFilter.GetArg((char *)"LOTTYPE"),       TFilter.GetArg((char *)"STEP"),
                TFilter.GetArg((char *)"ESPECID"),      TFilter.GetArg((char *)"AUTO_GOLDEN"),  TFilter.GetArg((char *)"TRANSACTIONID") );

    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::RMM_RmsRecipeUploadRep(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=RMS_RECIPE_UPLOAD_REP TIME_STAMP=2002/10/21-23:59:59.9 EQPID=WB01 
    //              RESULT=PASS RECIPEID=ADS12345-MS TRANSACTIONID=12 ERROR_CODE= ERROR_MSG=
    
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"RMS_RECIPE_UPLOAD_REP HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s RESULT=%s "
                        "RECIPEID=\"%s\" REPLY_KEY=%s ERROR_CODE=%s ERROR_MSG=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),        TFilter.GetArg((char *)"RESULT"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"REPLY_KEY"),
                TFilter.GetArg((char *)"ERROR_CODE"),   TFilter.GetArg((char *)"ERROR_MSG") );

    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::RMM_EqpRecipeDownload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=EQP_RECIPE_DOWNLOAD TIME_STAMP=2002/10/21-23:59:59.9 EQPID=WB01 
    //              RECIPEID=ADS12345-MS TRANSACTIONID=12 GOLDEN_DOWNLOAD=TRUE

    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"EQP_RECIPE_DOWNLOAD HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s "
                        "RECIPEID=\"%s\" GOLDEN_DOWNLOAD=%s "
                        "LOTID=%s PARTNO=%s LOTTYPE=%s STEP=%s TRANSACTIONID=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"GOLDEN_DOWNLOAD"),
                TFilter.GetArg((char *)"LOTID"),        TFilter.GetArg((char *)"PARTNO"),
                TFilter.GetArg((char *)"LOTTYPE"),      TFilter.GetArg((char *)"STEP"),
                TFilter.GetArg((char *)"TRANSACTIONID") );

    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::RMM_RmsRecipeDownloadRep(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=RMS_RECIPE_DOWNLOAD_REP TIME_STAMP=2002/10/21-23:59:59.9 EQPID=WB01 
    //              RECIPEID=ADS12345-MS TRANSACTIONID=12
    
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"RMS_RECIPE_DOWNLOAD_REP HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s RESULT=%s "
                        "RECIPEID=\"%s\" REPLY_KEY=%s ERROR_CODE=%s ERROR_MSG=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),        TFilter.GetArg((char *)"RESULT"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"REPLY_KEY"), 
                TFilter.GetArg((char *)"ERROR_CODE"),   TFilter.GetArg((char *)"ERROR_MSG") );

    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::RMM_FdcRecipeUpload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=FDC_RECIPE_UPLOAD TIME_STAMP=2002/10/21-23:59:59.9 EQPID=WB01 
    //              ACTION_MODE=V RECIPEID= ADS12345-MS MULTI=Y DMS_RECIPEID= ADS12345-001-NA ESPECID=ESPEC01 TRANSACTIONID=12
    
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"FDC_RECIPE_UPLOAD HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s ACTION_MODE=%s "
                        "RECIPEID=\"%s\" MULTI=%s DMS_RECIPEID=%s "
                        "LOTID=%s PARTNO=%s LOTTYPE=%s STEP=%s "
                        "TRANSACTIONID=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"ACTION_MODE"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"MULTI"),    TFilter.GetArg((char *)"DMS_RECIPEID"),
                TFilter.GetArg((char *)"LOTID"),        TFilter.GetArg((char *)"PARTNO"),   TFilter.GetArg((char *)"LOTTYPE"),       TFilter.GetArg((char *)"STEP"),
                TFilter.GetArg((char *)"TRANSACTIONID") );


    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}
bool CWorkflow::RMM_EqpFileUpload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      CMD=FDC_RECIPE_UPLOAD TIME_STAMP=2002/10/21-23:59:59.9 EQPID=WB01 
    //              ACTION_MODE=V RECIPEID= ADS12345-MS MULTI=Y DMS_RECIPEID= ADS12345-001-NA ESPECID=ESPEC01 TRANSACTIONID=12
    
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"EQP_FILE_UPLOAD HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s ACTION_MODE=%s "
                        "RECIPEID=\"%s\" MULTI=%s DMS_RECIPEID=%s "
                        "LOTID=%s PARTNO=%s LOTTYPE=%s STEP=%s "
                        "TRANSACTIONID=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"ACTION_MODE"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"MULTI"),    TFilter.GetArg((char *)"DMS_RECIPEID"),
                TFilter.GetArg((char *)"LOTID"),        TFilter.GetArg((char *)"PARTNO"),   TFilter.GetArg((char *)"LOTTYPE"),       TFilter.GetArg((char *)"STEP"),
                TFilter.GetArg((char *)"TRANSACTIONID") );


    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::RMM_GoldenRecipeUpload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      GOLDEN_RECIPE_UPLOAD TIME_STAMP=2018/04/25-10:26:16.2 EQPID=MSJ68 ACTION_MODE=VS RECIPEID="254FBGA_11.5X13_8X14_0.49_0824.ini" 
    //             MULTI= DMS_RECIPEID= LOTID=W4H15529 PARTNO=K4B4G1646D-BC000CV-WC74T4 LOTTYPE=PP AUTO_GOLDEN=YES TRANSACTIONID=4464936
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"GOLDEN_RECIPE_UPLOAD HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s ACTION_MODE=%s "
                        "RECIPEID=\"%s\" MULTI=%s DMS_RECIPEID=%s "
                        "LOTID=%s PARTNO=%s LOTTYPE=%s "
                        "AUTO_GOLDEN=%s TRANSACTIONID=%s REPLYSUBJECT=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"ACTION_MODE"),
                TFilter.GetArg((char *)"RECIPEID"),     TFilter.GetArg((char *)"MULTI"),         TFilter.GetArg((char *)"DMS_RECIPEID"),
                TFilter.GetArg((char *)"LOTID"),        TFilter.GetArg((char *)"PARTNO"),        TFilter.GetArg((char *)"LOTTYPE"),
                TFilter.GetArg((char *)"AUTO_GOLDEN"),  TFilter.GetArg((char *)"TRANSACTIONID"), TFilter.GetArg((char *)"REPLYSUBJECT") );


    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::RMM_GoldenRecipeDownload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    // MSG FORMAT
    //      GOLDEN_RECIPE_DOWNLOAD TIME_STAMP=2018/04/29-14:21:45.1 EQPID=MSJ32 RECIPEID="200FBGA_7.5X13.8_8X16_0.93_0525.ini" 
    //              GOLDEN_DOWNLOAD=TRUE LOTID=EUL2999R PARTNO=K4F4E6S4HF-BG000SS-ECO7TT LOTTYPE=PP TRANSACTIONID=4251914 
    CString TRet, TSendMsg;
    CFilter TFilter;
    CString TEqpID;

    TFilter.Reload( TMsg.GetPrintString() );
    TEqpID = TFilter.GetArg((char *)"EQPID");

    TSendMsg.Format((char *)"GOLDEN_RECIPE_DOWNLOAD HDR=(%s,%s,%s) "
                        "TIME_STAMP=%s EQPID=%s ACTION_MODE=%s "
                        "RECIPEID=\"%s\" GOLDEN_DOWNLOAD=%s "
                        "LOTID=%s PARTNO=%s LOTTYPE=%s "
                        "TRANSACTIONID=%s REPLYSUBJECT=%s ",
                m_pTEnv->GetString("RV_SUBJECT", "EES", DEFAULT_RV_EES_SUBJECT),
                m_pTEnv->GetString("RV_SUBJECT", "WORKFLOW", DEFAULT_RV_WORKFLOW_SUBJECT),
                TFilter.GetArg((char *)"EQPID"),
                GetDateTime_milisec(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"ACTION_MODE"),
                TFilter.GetArg((char *)"RECIPEID"),      TFilter.GetArg((char *)"GOLDEN_DOWNLOAD"),
                TFilter.GetArg((char *)"LOTID"),         TFilter.GetArg((char *)"PARTNO"),   TFilter.GetArg((char *)"LOTTYPE"),
                TFilter.GetArg((char *)"TRANSACTIONID"), TFilter.GetArg((char *)"REPLYSUBJECT") );


    if( EES_Send(TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet) == FALSE )
    {
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::SECS_RemoteCommand(CString TRCmdType, CString &TMsg, CString &TRetMsg, int nWaitReply /*= 0*/)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg, TCeid, TErrMsg;
    CString TRet;
    CString TEqpSubject;

    CString TProcess, TLine;
    CString TEqpID, TEqpModel;
    CString TResult;

    TProcess    = TFilter.GetArg((char *)"PROCESS");
    TLine       = TFilter.GetArg((char *)"LINE");
    TEqpID      = TFilter.GetArg((char *)"EQPID");
    TEqpModel   = TFilter.GetArg((char *)"EQPMODEL");

    TCeid       = TFilter.GetArg((char *)"CEID");
    TResult     = TFilter.GetArg((char *)"RESULT");
    TMsg        = TFilter.GetArg((char *)"TERMINAL_MSG");

    CRemoteCmdMsg TRCmd("REMOTE_COMMAND", nWaitReply);
    if ( TRCmdType == "WORK_REQ" || TRCmdType == "NEXT_WORK_REQ" || TRCmdType == "STOP_WORK_REQ")
    {
        if ( TResult == "PASS" )
        {
            TRCmd.SetRemoteCmd("NEXT_WORK_REQ", nWaitReply);
            TRCmd.Add("CUR_STEP_CEID",      TCeid,      "A");
        }else
        {
            TRCmd.SetRemoteCmd("STOP_WORK_REQ", nWaitReply);
            TRCmd.Add("CUR_STEP_CEID",      TCeid,      "A");
        }
    }

    if ( TMsg.Length() > 0 )
    {
        TRCmd.DispMsg( TMsg );
    }

    TBuf.Format((char *)"CMD=%s PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s %s",
            TRCmd.GetMsgName().GetPrintString(), 
            TProcess.GetPrintString(), TLine.GetPrintString(), TEqpModel.GetPrintString(), TEqpID.GetPrintString(), 
            TRCmd.GetPrintString().GetPrintString());

    TEqpSubject = GetEqpSubject(TEqpID);

    if( EQP_Send( TEqpSubject, TBuf, TArg, TArg, TArg, TRet, nWaitReply) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, "Remote Command Send NG");
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::SECS_CarrierID_Verification(CString &TMsg, CString &TRetMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg, TCeid, TErrMsg;
    CString TRet;
    CString TEqpSubject;

    CString TProcess, TLine;
    CString TEqpID, TEqpModel;
    CString TCarrierID, TMsgName, TCarrierAction, TPTN, TAttrList;

    TCeid       = TFilter.GetArg((char *)"CEID");

    TProcess    = TFilter.GetArg((char *)"PROCESS");
    TLine       = TFilter.GetArg((char *)"LINE");
    TEqpID      = TFilter.GetArg((char *)"EQPID");
    TEqpModel   = TFilter.GetArg((char *)"EQP_MODEL");

    TCarrierID  = TFilter.GetArg((char *)"CARRIERID");

    TMsgName   = "S3F17_TYPE1";
    TCarrierAction = "ProceedWithCarrier";
    TPTN       = "1";
    TAttrList  = "0";

//    TBuf.Format((char *)"CMD=SECS-II MSG_NAME=S3F17_TYPE1 PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s CARRIERACTION=%s CARRIERID=%s PTN=%s ATTRLIST=%s ATTRID1=%s ATTRDATA1=%s ATTRID2=%s ATTRDATA2=%s ATTRID3=%s ATTRDATA3=%s ATTRID4=%s ATTRDATA4=%s ATTRID5=%s ATTRDATA5=%s ", 
    TBuf.Format((char *)"CMD=SECS-II PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s MSG_NAME=%s CARRIERACTION=%s CARRIERID=%s PTN=%s ATTRLIST=%s ", 
            TProcess.GetPrintString(), TLine.GetPrintString(), TEqpModel.GetPrintString(), TEqpID.GetPrintString(), 
            TMsgName.GetPrintString(), TCarrierAction.GetPrintString(), TCarrierID.GetPrintString(), TPTN.GetPrintString(), TAttrList.GetPrintString() );

    TEqpSubject = GetEqpSubject(TEqpID);

    if( EQP_Send( TEqpSubject, TBuf, TArg, TArg, TArg, TRet, RV_REPLY_ON) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, "CarrierID_Verification Send NG");
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::SECS_SlotMap_Verification(CString &TMsg, CString &TRetMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg, TCeid, TErrMsg;
    CString TRet;
    CString TEqpSubject;

    int bFlowFlag = TRUE;

    CString TProcess, TLine;
    CString TEqpID, TEqpModel;
    CString TCarrierID, TMsgName, TCarrierAction, TPTN;
    CString TWaferLotId, TWaferList, TSLotNo;

    CString TAttr1, TAttr2, TAttr3, TAttr4, TAttr5;
    CString TAttrData1, TAttrData2, TAttrData3, TAttrData4, TAttrData5;

    TCeid       = TFilter.GetArg((char *)"CEID");

    TProcess    = TFilter.GetArg((char *)"PROCESS");
    TLine       = TFilter.GetArg((char *)"LINE");
    TEqpID      = TFilter.GetArg((char *)"EQPID");
    TEqpModel   = TFilter.GetArg((char *)"EQPMODEL");

    TCarrierID  = TFilter.GetArg((char *)"CARRIERID");
    TWaferLotId = TFilter.GetArg((char *)"LOTID");
    TWaferList  = TFilter.GetArg((char *)"WAFERINFO");
    TSLotNo     = TFilter.GetArg((char *)"SLOTNUMBER");

    TMsgName    = "S3F17_TYPE2";
    TCarrierAction = "ProceedWithCarrier";
    TPTN        = "1";
    TAttr1      = "Capacity";
    TAttrData1  = "25";
    TAttr2      = "SubstrateCount";
    TAttrData2  = "1";
    TAttr3      = "ContentMap";
    TAttrData3  = ":,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:";
    TAttr4      = "SlotMap";
    TAttrData4  = "2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2";
    TAttr5      = "Usage";
    TAttrData5  = "PRODUCT";

    if( bFlowFlag == TRUE )
    {
        //m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, "Waferdisplay [%s][%s][%s]", TWaferLotId.GetPrintString(), TWaferList.GetPrintString(), TSLotNo.GetPrintString() );

        CAceStrToken stkWaferList( TWaferList.GetPrintString(), "," );
        CAceStrToken stkSLotNo( TSLotNo.GetPrintString(), ":" );
        CAceStrToken stkSubWaferInfo;

        int nSlotIndex;
        CString TTokenValSLot, TTokenValWaferInfo, TWaferIDVal, TLotNoVal, TSLotVal;

        TAttrData2.Format("%d", stkSLotNo.GetCount());
        TAttrData3 = "";
        TAttrData4 = "";
        nSlotIndex = 0;

        for( int iLoop = 0; iLoop < 25; iLoop++ )
        {
            TSLotVal        = "2";
            TWaferIDVal     = "";
            TLotNoVal       = "";

            if ( nSlotIndex < stkSLotNo.GetCount() )
            {
                TTokenValSLot  = stkSLotNo.GetToken(nSlotIndex);
                if ( atoi(TTokenValSLot.GetPrintString()) == iLoop + 1 )
                {
                    TSLotVal = "4";
                    TTokenValWaferInfo  = stkWaferList.GetToken(nSlotIndex);
                    stkSubWaferInfo.Init( TTokenValWaferInfo.GetPrintString(), ":" );

                    if (stkSubWaferInfo.GetCount() > 0)
                    {
                        TWaferIDVal = stkSubWaferInfo.GetToken(0);
                        TLotNoVal   = TWaferLotId;
                    }

                    nSlotIndex++;
                }
            }

            if( TAttrData3.Length() <= 0 )
                TAttrData3.Format((char *)"%s:%s", TLotNoVal.GetPrintString(), TWaferIDVal.GetPrintString());
            else
            {
                TBuf.Format((char *)",%s:%s", TLotNoVal.GetPrintString(), TWaferIDVal.GetPrintString());
                TAttrData3 += TBuf;
            }

            if( TAttrData4.Length() <= 0 )
                TAttrData4.Format((char *)"%s", TSLotVal.GetPrintString());
            else
            {
                TBuf.Format((char *)",%s", TSLotVal.GetPrintString());
                TAttrData4 += TBuf;
            }
        }
    }

    TBuf.Format((char *)"CMD=SECS-II PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s MSG_NAME=%s CARRIERACTION=%s CARRIERID=%s PTN=%s "
                        "ATTRID1=%s ATTRDATA1=%s ATTRID2=%s ATTRDATA2=%s ATTRID3=%s ATTRDATA3=%s ATTRID4=%s ATTRDATA4=%s ATTRID5=%s ATTRDATA5=%s ", 
            TProcess.GetPrintString(), TLine.GetPrintString(), TEqpModel.GetPrintString(), TEqpID.GetPrintString(), 
            TMsgName.GetPrintString(), TCarrierAction.GetPrintString(), TCarrierID.GetPrintString(), TPTN.GetPrintString(), 
            TAttr1.GetPrintString(), TAttrData1.GetPrintString(), TAttr2.GetPrintString(), TAttrData2.GetPrintString(), TAttr3.GetPrintString(), TAttrData3.GetPrintString(), TAttr4.GetPrintString(), TAttrData4.GetPrintString(), TAttr5.GetPrintString(), TAttrData5.GetPrintString() );

    TEqpSubject = GetEqpSubject(TEqpID);

    if( EQP_Send( TEqpSubject, TBuf, TArg, TArg, TArg, TRet, RV_REPLY_ON) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, "SlotMap_Verification Send NG");
        return FALSE;
    }

    return TRUE;
}

bool CWorkflow::SECS_ProcessJob_Create(CString &TMsg, CString &TRetMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg, TCeid, TErrMsg;
    CString TRet;
    CString TEqpSubject;
    CFilter TMsgFilter;

    int bFlowFlag = TRUE;

    CString TProcess, TLine;
    CString TEqpID, TEqpModel;
    CString TLotID, TCarrierID, TMsgName;
    CString TWaferLotId, TWaferList, TSLotNo;
    CString TPRJobID, TPRJobList, TJobIDSeq, TMF, TCarrierList, TPRRecipeMethod, TRCPSpec, TRCPPara, TPRProcessStart;        // PR Job

    TCeid       = TFilter.GetArg((char *)"CEID");

    TProcess    = TFilter.GetArg((char *)"PROCESS");
    TLine       = TFilter.GetArg((char *)"LINE");
    TEqpID      = TFilter.GetArg((char *)"EQPID");
    TEqpModel   = TFilter.GetArg((char *)"EQPMODEL");

    TCarrierID  = TFilter.GetArg((char *)"CARRIERID");
    TLotID      = TFilter.GetArg((char *)"LOTID");
    TWaferLotId = TFilter.GetArg((char *)"LOTID");
    TWaferList  = TFilter.GetArg((char *)"WAFERINFO");
    TSLotNo     = TFilter.GetArg((char *)"SLOTNUMBER");
    TSLotNo.Replace( ':', ',');

    TJobIDSeq   = TFilter.GetArg((char *)"JOBIDSEQ");       // GetDateTime() + 8;
    TRCPSpec    = TFilter.GetArg((char *)"RCPSPEC");


    if( bFlowFlag == TRUE )
    {
        // PR Job Create
        TMsgName        = "S16F15_TYPE1";
        TPRJobList      = "1";
        TPRJobID.Format("%s_%s", TLotID.GetPrintString(), TJobIDSeq.GetPrintString());
        TMF             = "13";                 // Material Format Code (2=Casette/13=Carriers)
        TCarrierList    = "1";
        TPRRecipeMethod = "1";                  // RecipeMethod (1=RecipeOnly)
        TRCPPara        = ":0:F8";
        TPRProcessStart = "T";                  // PRPROCESSSTART (TRUE=AutomaticStart / FALSE=ManualStart)

        TBuf.Format((char *)"CMD=SECS-II PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s MSG_NAME=%s PRJOBLIST=%s PRJOBID=%s MF=%s CARRIERLIST=%s  "
                            "CARRIERID=%s SLOT=%s PRRECIPEMETHOD=%s RCPSPEC=%s RCPPARA=%s PRPROCESSSTART=%s ",
                TProcess.GetPrintString(), TLine.GetPrintString(), TEqpModel.GetPrintString(), TEqpID.GetPrintString(), 
                TMsgName.GetPrintString(), TPRJobList.GetPrintString(), TPRJobID.GetPrintString(), TMF.GetPrintString(), TCarrierList.GetPrintString(),
                TCarrierID.GetPrintString(), TSLotNo.GetPrintString(), TPRRecipeMethod.GetPrintString(), TRCPSpec.GetPrintString(), TRCPPara.GetPrintString(), TPRProcessStart.GetPrintString() );

        TEqpSubject = GetEqpSubject(TEqpID);

        if( EQP_Send( TEqpSubject, TBuf, TArg, TArg, TArg, TRet, RV_REPLY_ON) == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, "PR Job Create Send NG");
            return FALSE;
        }
    }

    return TRUE;
}

bool CWorkflow::SECS_ControlJob_Create(CString &TMsg, CString &TRetMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg, TCeid, TErrMsg;
    CString TTrackingMsg, TRet;
    CString TEqpSubject;
    CFilter TMsgFilter;

    int bFlowFlag = TRUE;

    CString TProcess, TLine;
    CString TEqpID, TEqpModel;
    CString TLotID, TCarrierID, TMsgName;
    CString TPRJobID, TJobIDSeq;                        // PR Job
    CString TOBJSpec, TOBJType, TAttrList;              // Control Job
    CString TAttr1, TAttr2, TAttr3, TAttr4, TAttr5, TAttr6;
    CString TAttrData1, TAttrData2, TAttrData3, TAttrData4, TAttrData5, TAttrData6;

    TCeid       = TFilter.GetArg((char *)"CEID");

    TProcess    = TFilter.GetArg((char *)"PROCESS");
    TLine       = TFilter.GetArg((char *)"LINE");
    TEqpID      = TFilter.GetArg((char *)"EQPID");
    TEqpModel   = TFilter.GetArg((char *)"EQPMODEL");

    TCarrierID  = TFilter.GetArg((char *)"CARRIERID");
    TLotID      = TFilter.GetArg((char *)"LOTID");
    TJobIDSeq   = TFilter.GetArg((char *)"JOBIDSEQ");   // GetDateTime() + 8;

    TPRJobID.Format("%s_%s", TLotID.GetPrintString(), TJobIDSeq.GetPrintString());

    if( bFlowFlag == TRUE )
    {
        // Control Job Create
        TMsgName    = "S14F9_TYPE1";
        TOBJSpec    = "";
        TOBJType    = "ControlJob";
        TAttrList   = "6";
        TAttr1      = "ObjID";
        TAttrData1.Format("%s_%s", TCarrierID.GetPrintString(), TJobIDSeq.GetPrintString());     // Control Job Name
        TAttr2      = "CarrierInputSpec";  
        TAttrData2  = TCarrierID;
        TAttr3      = "MtrlOutSpec";
        TAttrData3  = "";
        TAttr4      = "ProcessingCtrlSpec";
        TAttrData4  = TPRJobID;
        TAttr5      = "ProcessOrderMgmt";
        TAttrData5  = "1";                  // ProcessOrderMgmt (1=ARRIVAL / 2=OPTIMIZE / 3=LIST)
        TAttr6      = "StartMethod";
        TAttrData6  = "T";

        TBuf.Format((char *)"CMD=SECS-II PROCESS=%s LINE=%s EQPMODEL=%s EQPID=%s MSG_NAME=%s OBJSPEC=%s OBJTYPE=%s ATTRLIST=%s "
                            "ATTRID1=%s ATTRDATA1=%s ATTRID2=%s ATTRDATA2=%s ATTRID3=%s ATTRDATA3=%s ATTRID4=%s ATTRDATA4=%s ATTRID5=%s ATTRDATA5=%s ATTRID6=%s ATTRDATA6=%s ",
                TProcess.GetPrintString(), TLine.GetPrintString(), TEqpModel.GetPrintString(), TEqpID.GetPrintString(), 
                TMsgName.GetPrintString(), TOBJSpec.GetPrintString(), TOBJType.GetPrintString(), TAttrList.GetPrintString(),
                TAttr1.GetPrintString(), TAttrData1.GetPrintString(), TAttr2.GetPrintString(), TAttrData2.GetPrintString(), TAttr3.GetPrintString(), TAttrData3.GetPrintString(),
                TAttr4.GetPrintString(), TAttrData4.GetPrintString(), TAttr5.GetPrintString(), TAttrData5.GetPrintString(), TAttr6.GetPrintString(), TAttrData6.GetPrintString() );

        TEqpSubject = GetEqpSubject(TEqpID);

        if( EQP_Send( TEqpSubject, TBuf, TArg, TArg, TArg, TRet, RV_REPLY_ON ) == FALSE )
        {
            m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_EQP, "Control Job Create Send NG");
            return FALSE;
        }
    }

    return TRUE;
}

bool CWorkflow::WaferMap_Download(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf;
    CString TTrackingMsg, TRet;
    CFilter TMsgFilter;

    //CMD=WAFER_MAP_SETUP PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s MID=%s IDTYP=%d MAPFT=%d FNLOC=%s FFROT=%s ORLOC=%d PRAXI=%d BCEQU=%s NULBC=%s MAPKIND=%s
    CString TSystemByte = TFilter.GetArg((char *)"SYSTEMBYTE"   );
    CString TMID        = TFilter.GetArg((char *)"MID"          );
    CString TIDTYP      = TFilter.GetArg((char *)"IDTYP"        );
    CString TMAPFT      = TFilter.GetArg((char *)"MAPFT"        );
    CString TFNLOC      = TFilter.GetArg((char *)"FNLOC"        );
    CString TFFROT      = TFilter.GetArg((char *)"FFROT"        );
    CString TORLOC      = TFilter.GetArg((char *)"ORLOC"        );
    CString TPRAXI      = TFilter.GetArg((char *)"PRAXI"        );
    CString TBCEQU      = TFilter.GetArg((char *)"BCEQU"        );
    CString TNULBC      = TFilter.GetArg((char *)"NULBC"        );
    CString TMapKind    = TFilter.GetArg((char *)"MAPKIND"      );
    CString TReverse        = TFilter.GetArg((char *)"REVERSEYN"    );
    CString TReverseAxis    = TFilter.GetArg((char *)"REVERSEAXIS"  );

    if ( TNULBC == NULL )
    {
        TNULBC.Format(" ");
    }

    // CMD=MAP_DOWNLOAD MID=S1HW1-14 IDTYP=0 MAPFT=0 FNLOC=180 FFROT=0 ORLOC=4 PRAXI=3 BCEQU=1234 NULBC=" " REPLY_SUBJECT=AAAA.EEE O2CP.TCASSY.COC.WF
    TBuf.Format((char *)"CMD=MAP_DOWNLOAD PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s MID=%s IDTYP=%s MAPFT=%s FNLOC=%s FFROT=%s ORLOC=%s PRAXI=%s BCEQU=%s NULBC=\"%s\" MAPKIND=%s REVERSEYN=%s REVERSEAXIS=%s ",
                TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"), TFilter.GetArg((char *)"EQPID"), 
                TMID.GetPrintString(), TIDTYP.GetPrintString(), TMAPFT.GetPrintString(),
                TFNLOC.GetPrintString(), TFFROT.GetPrintString(), TORLOC.GetPrintString(),
                TPRAXI.GetPrintString(), TBCEQU.GetPrintString(), TNULBC.GetPrintString(), TMapKind.GetPrintString(),
                TReverse.GetPrintString(), TReverseAxis.GetPrintString());

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    if( Inkless_Comm(TBuf, TBinMsg, TPPBodyMsg, TDmsMsg, TRet, RV_REPLY_ON) == FALSE )
    {
        return FALSE;
    }

    TMsgFilter.Reload(TRet.GetPrintString());

    TReplyMsg.Format("CMD=%s_REP SYSTEMBYTE=%s STATUS=%s PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s MID=%s ROWCT=%s COLCT=%s PDRCT=%s REPX1=%s REPY1=%s FINISHFLAG=%s BINLIST=\"%s\" MSG=\"%s\" ",
            TFilter.GetArg((char *)"CMD"            ),
            TSystemByte.GetPrintString(),
            TMsgFilter.GetArg((char *)"STATUS"      ),
            TMsgFilter.GetArg((char *)"PROCESS"     ),
            TMsgFilter.GetArg((char *)"LINE"        ),
            TMsgFilter.GetArg((char *)"EQP_MODEL"   ),
            TMsgFilter.GetArg((char *)"EQPID"       ),
            TMsgFilter.GetArg((char *)"MID"         ),
            TMsgFilter.GetArg((char *)"ROWCT"       ),
            TMsgFilter.GetArg((char *)"COLCT"       ),
            TMsgFilter.GetArg((char *)"PDRCT"       ),
            TMsgFilter.GetArg((char *)"REPX1"       ),
            TMsgFilter.GetArg((char *)"REPY1"       ),
            TMsgFilter.GetArg((char *)"FINISHFLAG"  ),
            TMsgFilter.GetArg((char *)"BINLIST"     ),
            TMsgFilter.GetArg((char *)"MSG"         ) );

    return TRUE;
}


bool CWorkflow::WaferMap_Upload(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf;
    CString TTrackingMsg, TRet;
    CFilter TMsgFilter;

    //CMD=WAFER_MAP_UPLOAD SYSTEMBYTE=%s PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s MID=%s IDTYP=%d REPX1=%s REPY1=%s BINLIST=\"%s\"
    CString TSystemByte     = TFilter.GetArg((char *)"SYSTEMBYTE"   );
    CString TMID            = TFilter.GetArg((char *)"MID"          );
    CString TIDTYP          = TFilter.GetArg((char *)"IDTYP"        );
    CString TREPx1          = TFilter.GetArg((char *)"REPX1"        );
    CString TREPy1          = TFilter.GetArg((char *)"REPY1"        );
    CString TFNLOC          = TFilter.GetArg((char *)"FNLOC"        );
    CString TFFRPT          = TFilter.GetArg((char *)"FFROT"        );
    CString TORGMAP         = TFilter.GetArg((char *)"ORGMAP"       );
    CString TBinList        = TFilter.GetArg((char *)"BINLIST"      );
    CString TReverse        = TFilter.GetArg((char *)"REVERSEYN"    );
    CString TReverseAxis    = TFilter.GetArg((char *)"REVERSEAXIS"  );

    if ( TFNLOC.Length() == 0 )
    {
        TFNLOC = "0";
    }

    if ( TFFRPT.Length() == 0 )
    {
        TFFRPT = "0";
    }

    if ( TORGMAP.Length() == 0 )
    {
        TORGMAP = "N";
    }

    // CMD=MAP_SAVE PROCESS=CHIPMNT LINE=CAS1 EQP_MODEL=FC3000L2 EQPID=ACB01A MID=GEG086-20 FNLOC=0 FFROT=0 BIN_LIST=""
    TBuf.Format((char *)"CMD=MAP_SAVE PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s MID=%s FNLOC=%s FFROT=%s ORGMAP=%s REVERSEYN=%s REVERSEAXIS=%s BIN_LIST=\"%s\" ",
                TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQP_MODEL"), TFilter.GetArg((char *)"EQPID"), 
                TMID.GetPrintString(), 
                TFNLOC.GetPrintString(), 
                TFFRPT.GetPrintString(),
                TORGMAP.GetPrintString(),
                TReverse.GetPrintString(), 
                TReverseAxis.GetPrintString(),
                TBinList.GetPrintString() );

    AddMsg( TFilter, TBuf, "PDRCT"  , "PDRCT"   );

    int nRvType = MESSAGE;

    if( TBinMsg.Length() > 0    ) nRvType |= MESSAGE_BINARY;
    if( TPPBodyMsg.Length() > 0 ) nRvType |= MESSAGE_PPBODY;
    if( TDmsMsg.Length() > 0    ) nRvType |= MESSAGE_DMS;

    if( Inkless_Comm(TBuf, TBinMsg, TPPBodyMsg, TDmsMsg, TRet, RV_REPLY_ON) == FALSE )
    {
        return FALSE;
    }

    TMsgFilter.Reload(TRet.GetPrintString());

    // CMD=MAP_SAVE_REP STATUS=PASS PROCESS=CHIPMNT LINE=CAS1 EQP_MODEL=FC3000L2 EQPID=ACB01A MID=GEG086-20 SEND_SUBJECT=O2CP.CATCTEST.MAP_AGENT
    TReplyMsg.Format("CMD=%s_REP SYSTEMBYTE=%s STATUS=%s PROCESS=%s LINE=%s EQP_MODEL=%s EQPID=%s MID=%s MSG=\"%s\" ",
            TFilter.GetArg((char *)"CMD"            ),
            TSystemByte.GetPrintString(),
            TMsgFilter.GetArg((char *)"STATUS"      ),
            TMsgFilter.GetArg((char *)"PROCESS"     ),
            TMsgFilter.GetArg((char *)"LINE"        ),
            TMsgFilter.GetArg((char *)"EQP_MODEL"   ),
            TMsgFilter.GetArg((char *)"EQPID"       ),
            TMsgFilter.GetArg((char *)"MID"         ),
            TMsgFilter.GetArg((char *)"MSG"         ) );

    return TRUE;
}


void CWorkflow::Rotaion_FrameMap( CString T2DOrigin, MAPINFO *pMap )
{
    CString TRotationMap("");
    char *pRowStartPos;
    int i, j;

    if ( pMap->TMapData.Length() <= 0 ) 
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Rotaion_FrameMap() :: MapData Length is zero");
        return;
    }

    if ( pMap->nTotalRow * pMap->nTotalCol != pMap->TMapData.Length() )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Rotaion_FrameMap() :: TotalRow(%d) * TotalCol(%d) != MapData Length(%d)",  pMap->nTotalRow, pMap->nTotalCol, pMap->TMapData.Length() );
        return;
    }

    if      ( T2DOrigin == "LOWERRIGHT" || T2DOrigin == "4" )
    {
        for ( i = pMap->nTotalRow - 1; i >= 0; i--)
        {
            pRowStartPos = pMap->TMapData.GetPrintString() + (i * pMap->nTotalCol);

            for ( j = pMap->nTotalCol-1; j >= 0; j--)
            {
                TRotationMap += CString( pRowStartPos + j, 1);
            }
        }

        pMap->TMapData = TRotationMap;
    }
    else if ( T2DOrigin == "LOWERLEFT" || T2DOrigin == "3" )
    {
        for ( i = pMap->nTotalRow - 1; i >= 0; i--)
        {
            pRowStartPos = pMap->TMapData.GetPrintString() + (i * pMap->nTotalCol);

            for ( j = 0; j < pMap->nTotalCol; j++)
            {
                TRotationMap += CString( pRowStartPos + j, 1);
            }
        }

        pMap->TMapData = TRotationMap;
    }
    else if ( T2DOrigin == "UPPERRIGHT" || T2DOrigin == "1" )
    {
        for ( i = 0; i < pMap->nTotalRow; i++)
        {
            pRowStartPos = pMap->TMapData.GetPrintString() + (i * pMap->nTotalCol);

            for ( j = pMap->nTotalCol-1; j >= 0; j--)
            {
                TRotationMap += CString( pRowStartPos + j, 1);
            }
        }

        pMap->TMapData = TRotationMap;
    }
}

void CWorkflow::Rotaion_DeviceID( CString T2DOrigin, int nCols, int nRows, CString &TDeviceID)
{

    CAceStrToken stDeviceInfo;
    CString TDevideIDList[1500], TDeviceIDListTemp[1500], TTemp;;
    int i, j, nDeviceInfo,  nMaxIndex, nStartIndex, nCurIndex;
        
    if (TDeviceID[0] == ',' )
    {
        TTemp = "-";
        TTemp += TDeviceID;
        
        TDeviceID = TTemp;
    }

    if (TDeviceID[TDeviceID.Length() - 1] == ',' )
    {
        TDeviceID += "-";
    }
        
    TDeviceID.ReplaceStr(",,",",-,");

    m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, " Rotaion_DeviceID() :: CHANGE TDeviceID[%s]", TDeviceID.GetPrintString());

    stDeviceInfo.Init( TDeviceID.GetPrintString(), "," );
    nDeviceInfo = stDeviceInfo.GetCount();    

    nMaxIndex = nCols * nRows; 

    if (nDeviceInfo != nMaxIndex)
    {
        TDeviceID = "";
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Rotaion_DeviceID() :: MapData Length ERROR DEVICECNT=[%d], TOTALCNT=[%d]", nDeviceInfo,  nMaxIndex);
        return ;
    }
        
    for (i = 0 ; i < nDeviceInfo ; i++ )
    {
        TDevideIDList[i] = stDeviceInfo.GetToken(i);
    }

    if ( T2DOrigin == "LOWERRIGHT" || T2DOrigin == "4" )
    {
        nStartIndex = nMaxIndex;
        for (i = 0; i < nMaxIndex ; i++ )
        {
            nStartIndex--;
            TDeviceIDListTemp[i] = TDevideIDList[nStartIndex];
        }
    }
    else if ( T2DOrigin == "LOWERLEFT" || T2DOrigin == "3" )
    {
        nCurIndex = 0;
        for (i = nRows - 1; i >=0; i--)
        {
            nStartIndex = i * nCols;
                        
            for (j = 0; j < nCols ; j++ )
            {
                TDeviceIDListTemp[nCurIndex] = TDevideIDList[nStartIndex + j];
                nCurIndex++;
            }
        }     
    }
    else if ( T2DOrigin == "UPPERRIGHT" || T2DOrigin == "1" )
    {   
        nCurIndex = 0;  
        
        for (i = 0; i < nRows ; i++ )
        {
            nStartIndex = i * nCols;

            for (j = nCols - 1; j >= 0 ; j--)
            {
                TDeviceIDListTemp[nCurIndex] = TDevideIDList[nStartIndex + j];
                nCurIndex++;
            }
        }     
    }
    
    TDeviceID = "";

    for ( i = 0 ; i < nDeviceInfo ; i++ )
    {
        TDeviceID += TDeviceIDListTemp[i];
        TDeviceID += ",";
    }

    TDeviceID[TDeviceID.Length() - 1]       = NULL; 

    TDeviceID.ReplaceStr("-","");

}

void CWorkflow::Rotaion_ChipInfo( CString T2DOrigin, int nCols, int nRows, CString &TChipIdInfo, CString &TEqpChipIdInfo, CString &TCellStatus)
{
    CString TTemp, TMatInfo;
    CAceStrToken stChipInfo, stXYLoc;
    CString TPkgIDList[1500], TPkgIDListTemp[1500];
    CString TXLoc[1500], TXLocTemp[1500];
    CString TYLoc[1500], TYLocTemp[1500];
    CString TCellList[1500], TCellListTemp[1500];

    int i, nChipInfo, nXyCnt, nMaxIndex, nTemp, nMapDetailListCount;

//Row 3
//Col 5
//0    
//
//01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
//15 14 13 12 11 10 09 08 07 06 05 04 03 02 01
//
//0( 2  => 14 ( 2 * 5 + 4 )
//1 => 13 ( 2 * 5 + 3 )
//2 => 12 ( 2 * 5 + 2 )
//    1,2:PKG2D;1,3:PKG2D
    
    nMapDetailListCount = -1;  
        
    if (strncmp(TChipIdInfo.GetPrintString() , "MAP_DETAIL_LIST_COUNT",  21 ) == 0) 
    {
//        MAP_DETAIL_LIST_COUNT=12 
//        MAP_DETAIL_1="B_ID=BOAT1111 B_X=1 B_Y=1 B_CELL_BIN=PKG101 B_CELL_STATUS=1 C_ID=BOAT2222 C_X=1 C_Y=1 C_CELL_BIN=PKG101 HEADER_NO=2 CHIP_STACK_CNT=1 " 

        CFilter TMapDetailFilter(TChipIdInfo.GetPrintString());
        CFilter TMapDetailValueFilter;

        TTemp  = TMapDetailFilter.GetArg((char *)"MAP_DETAIL_LIST_COUNT");     if ( TTemp.Length() == 0 ) { TTemp = "0"; }

        nMapDetailListCount = atoi( TTemp.GetPrintString() );

        for ( i = 0 ; i < nMapDetailListCount ; i++ )
        {
            TTemp.Format("MAP_DETAIL_%d", i + 1);
            TTemp = TMapDetailFilter.GetArg(TTemp.GetPrintString());
            TMapDetailValueFilter.Reload(TTemp.GetPrintString());
            TXLoc[i]        = TMapDetailValueFilter.GetArg("B_X");
            TYLoc[i]        = TMapDetailValueFilter.GetArg("B_Y");
            TPkgIDList[i]   = TMapDetailValueFilter.GetArg("B_CELL_BIN");
            TCellList[i]    = TMapDetailValueFilter.GetArg("B_CELL_STATUS");
        }

    }
    else
    {    
        stChipInfo.Init( TChipIdInfo.GetPrintString(), ";" );
        nChipInfo = stChipInfo.GetCount();    
        for (i = 0 ; i < nChipInfo ; i++ )
        {
            TMatInfo = stChipInfo.GetToken(i);
            stXYLoc.Init(TMatInfo.GetPrintString(), ",");
            nXyCnt = stXYLoc.GetCount();
            
            if (nXyCnt == 2)
            {
                TXLoc[i]        = stXYLoc.GetToken(0);

                TTemp           = stXYLoc.GetToken(1);
                TYLoc[i]        = TTemp.Left(TTemp.Find(':', 0) );
                TPkgIDList[i]   = TTemp.Mid(TTemp.Find(':', 0) + 1 );
            }
            else
            {
                TXLoc[i]        = "-";
                TYLoc[i]        = "-";
                TPkgIDList[i]   = "-";
            }
        }
    }

    nMaxIndex = nCols * nRows; 

    if ( T2DOrigin == "LOWERRIGHT" || T2DOrigin == "4" )
    {
        for (i = 0; i < nMaxIndex ; i++ )
        {
            TTemp.Format("%d", nCols + 1 - atoi(TXLoc[i].GetPrintString()) );
            TXLoc[i] = TTemp;

            TTemp.Format("%d", nRows + 1 - atoi(TYLoc[i].GetPrintString()) );
            TYLoc[i] = TTemp;
        }
    }
    else if ( T2DOrigin == "LOWERLEFT" || T2DOrigin == "3" )
    {
        for (i = 0; i < nMaxIndex ; i++ )
        {
            TTemp.Format("%d", nRows + 1 - atoi(TYLoc[i].GetPrintString()) );
            TYLoc[i] = TTemp;
        }     
    }
    else if ( T2DOrigin == "UPPERRIGHT" || T2DOrigin == "1" )
    {
       
        for (i = 0; i < nMaxIndex ; i++ )
        {
            TTemp.Format("%d", nCols + 1 - atoi(TXLoc[i].GetPrintString()) );
            TXLoc[i] = TTemp;
        }     
    }
    
    for (i = 0 ; i < nMaxIndex  ; i++)
    {        
        nTemp = (atoi(TYLoc[i].GetPrintString()) - 1) * nCols +  (atoi(TXLoc[i].GetPrintString()) - 1);
        
        TPkgIDListTemp[nTemp] = TPkgIDList[i];

        if ( nMapDetailListCount > 0)
        {
            TCellListTemp[nTemp] = TCellList[i];
        }

        TXLocTemp[nTemp] = TXLoc[i];
        TYLocTemp[nTemp] = TYLoc[i];
    }
    
    TEqpChipIdInfo  = "";
    TChipIdInfo     = "";
    TCellStatus     = "";

    for (i = 0 ; i < nMaxIndex  ; i++)
    {        
        TEqpChipIdInfo += TPkgIDListTemp[i];
        TEqpChipIdInfo += ",";

        TTemp.Format("%s,%s,%s;", TXLocTemp[i].GetPrintString(), TYLocTemp[i].GetPrintString(), TPkgIDListTemp[i].GetPrintString());
        TChipIdInfo += TTemp;

        if ( nMapDetailListCount > 0)
        {
            TCellStatus += TCellListTemp[i];
        }

    }
    
    TEqpChipIdInfo[TEqpChipIdInfo.Length() - 1] = NULL; 
    TChipIdInfo[TChipIdInfo.Length() - 1]       = NULL; 

}



int CWorkflow::GetMapXPosRotation(int nXPos, int nMax_X, CString T2DOrigin)
{
    int nTempXpos, nReverseXPos;

    if      (T2DOrigin == "LOWERRIGHT" || T2DOrigin == "4")
    {
        nTempXpos = (nMax_X + 1) - nXPos;
    }
    else if (T2DOrigin == "LOWERLEFT" || T2DOrigin == "3")
    {
        nTempXpos = nXPos;
    }
    else if (T2DOrigin == "UPPERRIGHT" || T2DOrigin == "1")
    {
        nTempXpos = (nMax_X + 1) - nXPos;
    }
    else
    {
        nTempXpos = nXPos;
    }

    return nTempXpos;
}

int CWorkflow::GetMapYPosRotation(int nYPos, int nMax_Y, CString T2DOrigin)
{
    int nTempYpos;

    if      (T2DOrigin == "LOWERRIGHT" || T2DOrigin == "4") 
    {
        nTempYpos = (nMax_Y + 1) - nYPos;
    }
    else if (T2DOrigin == "LOWERLEFT" || T2DOrigin == "3")  
    {
        nTempYpos = (nMax_Y + 1) - nYPos;
    }
    else if (T2DOrigin == "UPPERRIGHT" || T2DOrigin == "1") 
    {
        nTempYpos = nYPos;
    }
    else
    {
        nTempYpos = nYPos;
    }

    return nTempYpos;
}

bool CWorkflow::EQP_DispMsg(CString TSubject, CString TMsg)
{
    CString TSendMsg;

    TSendMsg.Format((char *)"CMD=DISP_MSG MSG=\"%s\" ", TMsg.GetPrintString());
        
    int     nRet;  
    CString TRet, TBinMsg, TPPBodyMsg, TDmsMsg;
    
    nRet = EQP_Send( TSubject, TSendMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TRet);
    if( nRet == FALSE ) 
    {
        return FALSE;
    }
       
    return TRUE;
}

bool CWorkflow::Get_Mats_Current(CString &TMsg, CString TEqpID, CString &TRetVal, CString TMatType)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg;
    CString TSimaxData, TRet;
    CString TMatInfo, TMatLotID, TMatDesc, TMatName;
    CFilter TMESFilter, TMsgFilter;

    TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s OPERID=%s SQL_ID=IMS_SQL_0040 VALUE=%s", 
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TEqpID.GetPrintString(),
            "AUTO",
            TEqpID.GetPrintString() );
    if( MES_Comm("GET_SIMAXDATA", TBuf, TRet) == FALSE )
    {
        TRetVal.Format("CONSM_ID=%s CONSM_PROD_ID=%s CONSM_NAME=%s ", "", "", "" );
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_Mats_Current() :: GET_SIMAXDATA FAIL : Mat_Info=[%s]", TRet.GetPrintString());

        return FALSE;
    }

    TMESFilter.Reload(TRet.GetPrintString());
    TSimaxData = "";

    if( strcmp(TMESFilter.GetArg((char *)"STATUS"), "PASS") != 0 )
    {
        TRetVal.Format("CONSM_ID=%s CONSM_PROD_ID=%s CONSM_NAME=%s ", "", "", "" );
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_Mats_Current() :: GET_SIMAXDATA FAIL : Mat_Info=[%s]", TRet.GetPrintString());

        return FALSE;
    }//else
//    {
//        CString TMsgBuf    = TMESFilter.GetArg("MSG");
//        TMsgFilter.Reload ( TMsgBuf.GetPrintString() );
//        CString TIMSMsg    = TMsgFilter.GetArg("IMS_MSG");
//        TIMSMsg.Replace('<' , ' ');
//        TIMSMsg.Replace('>' , ' ');
//        TIMSMsg.Replace('[' , '"');
//        TIMSMsg.Replace(']' , '"');
//        TMsgFilter.Reload ( TIMSMsg.GetPrintString() );
//        TSimaxData = TMsgFilter.GetArg("SIMAXDATA");
//    }
    TSimaxData = GetArgfromMES(TMESFilter, "SIMAXDATA");

    CAceStrToken st( TSimaxData.GetPrintString(), "@#@" );

    int nCnt = st.GetCount();

    if (nCnt > 0)
    {
        for (int i = 0 ; i < nCnt ; i++)
        {
            TMatInfo = st.GetToken(i);

            CAceStrToken st2( TMatInfo.GetPrintString(), "#@#" );

            int nCnt2 = st2.GetCount();

            if (nCnt2 == 3)
            {
                TMatName  = st2.GetToken(2);

                if ( TMatName == TMatType )        // "EMC"
                {
                    TMatLotID = st2.GetToken(0);
                    TMatDesc  = st2.GetToken(1);
                    break;
                }else
                {
                    TMatLotID = "";
                    TMatDesc  = "";
                    TMatName  = "";
                }
            }else
            {
                TMatLotID = "";
                TMatDesc  = "";
                TMatName  = "";
            }
        }
    }else
    {
        TMatLotID = "";
        TMatDesc  = "";
        TMatName  = "";
    }

    TBuf.Format((char *)"CONSM_ID=%s CONSM_PROD_ID=%s CONSM_NAME=%s ", TMatLotID.GetPrintString(), TMatDesc.GetPrintString(), TMatName.GetPrintString() );

    TRetVal = TBuf;

    return TRUE;
}

bool CWorkflow::Get_Mats_RemainTime(CString &TMsg, CString TMatLotID, CString &TRetVal)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg, TModuleInfo, TModuleEqpID;
    CString TSimaxData, TRet;
    CString TMatRemainTime; 
    CFilter TMESFilter;
    CString TEqpModel   = TFilter.GetArg((char *)"EQP_MODEL");
    CString TEqpID = TFilter.GetArg((char *)"EQPID");

    if( TEqpModel == "WCM300LM" )
    {
        TModuleInfo     = TFilter.GetArg((char *)GetRPTInfo(TFilter, "MODULEINFO"   ).GetPrintString());
        TModuleEqpID.Format((char *)"%s%s", TFilter.GetArg((char *)"EQPID"), TModuleInfo.GetPrintString());
    }else
    {
        TModuleEqpID = TEqpID;
    }


    TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s OPERID=%s SQL_ID=IMS_SQL_0047 VALUE=%s", 
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TModuleEqpID.GetPrintString(),
            "AUTO",
            TMatLotID.GetPrintString() );
    if( MES_Comm("GET_SIMAXDATA", TBuf, TRet) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_MatRemainTime() :: GET_SIMAXDATA FAIL : TBuf=[%s]", TBuf.GetPrintString());
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_MatRemainTime() :: GET_SIMAXDATA FAIL : ERRMSG=[%s]", TRet.GetPrintString());

        return FALSE;
    }

    TRet.Replace('[',' ');
    TRet.Replace(']',' ');

    TMESFilter.Reload(TRet.GetPrintString());
    TMatRemainTime = "";

    if( strcmp(TMESFilter.GetArg((char *)"STATUS"), "PASS") != 0 )
    {
        TBuf.Format((char *)"%s ", TMESFilter.GetArg("ERRORMSG") );
        TRetVal = TBuf;
        return FALSE;
    }

    if ( strcmp(TMESFilter.GetArg((char *)"RESULT"), "PASS") != 0 )
    {
        TBuf.Format((char *)"%s ", TMESFilter.GetArg("ERRORMSG") );
        TRetVal = TBuf;

        return FALSE;
    }else
    {
        TMatRemainTime = TMESFilter.GetArg("REMAINTIME");
    }

    TBuf.Format((char *)"REMAINTIME=%s ", TMatRemainTime.GetPrintString() );
    TRetVal = TBuf;
    m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB1, "Get_Mats_RemainTime() :: TRetVal=[%s]", TRetVal.GetPrintString());

    return TRUE;
}

bool CWorkflow::Get_SetLotInfo(CString &TMsg, CString &TRetVal)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg;
    CString TSimaxData, TRet;
    CString TLotID, TSetType, TSetNo, TLotList;
    CFilter TMESFilter, TMsgFilter;

    TLotID      = TFilter.GetArg((char *)"LOTID"    );
    TSetType    = TFilter.GetArg((char *)"SET_TYPE" );

    TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s OPERID=%s SQL_ID=IMS_SQL_0037 VALUE=%s ", 
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQPID"),
            "AUTO",
            TLotID.GetPrintString() );

    if( MES_Comm("GET_SIMAXDATA", TBuf, TRet) == FALSE )
    {
        TRetVal.Format("SET_NO=%s LOT_LIST=%s ", "0", "" );
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_SetLotInfo() :: GET_SIMAXDATA FAIL : TBuf=[%s] ", TBuf.GetPrintString());

        return FALSE;
    }

    TMESFilter.Reload(TRet.GetPrintString());
    TSimaxData = "";

    if( strcmp(TMESFilter.GetArg((char *)"STATUS"), "PASS") != 0 )
    {
        TRetVal.Format("SET_NO=%s LOT_LIST=%s ", "0", "" );
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_SetLotInfo() :: GET_SIMAXDATA FAIL : TBuf=[%s] ", TBuf.GetPrintString());

        return FALSE;
    }

    TSimaxData = GetArgfromMES(TMESFilter, "SIMAXDATA");

    CAceStrToken st( TSimaxData.GetPrintString(), "#@#" );

    int nCnt = st.GetCount();

    if (nCnt > 0)
    {
        for (int i = 0 ; i < nCnt ; i++)
        {
            if ( i == 0)
            {
                TSetNo      = st.GetToken(i);
            }else
            {
                if( TLotList.Length() <= 0 )
                    TLotList    = st.GetToken(i);
                else
                {
                    TBuf.Format((char *)",%s", st.GetToken(i) );
                    TLotList += TBuf;
                }
            }
        }
    }else
    {
        TSetNo      = "0";
        TLotList    = "";
    }

    TBuf.Format((char *)"SET_NO=%s LOT_LIST=\"%s\" ", TSetNo.GetPrintString(), TLotList.GetPrintString() );

    TRetVal = TBuf;

    return TRUE;
}

bool CWorkflow::Get_PCBList(CString &TMsg, CString &TLotID, CString &TRetVal)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg;
    CString TRet, TPcbList, TPcbCount, TPMSCheck;
    CFilter TMESFilter, TMsgFilter;

    TRetVal = "N";

    TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s OPERID=%s FRAMEID=%s INFO_TYPE=%s", 
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"),  TFilter.GetArg((char *)"EQPID"),
            "AUTO",
            TLotID.GetPrintString(),
            "LOT" );
    if( MES_Comm("FRAMEDISPLAY", TBuf, TRet) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_PCBList() :: IMS_FRAMEDISPLAY FAIL : TBuf=[%s]", TBuf.GetPrintString());
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, " Get_PCBList() :: IMS_FRAMEDISPLAY FAIL : ERRMSG=[%s]", TRet.GetPrintString());

        TRetVal.Format("FRAMELIST=%s PCBCOUNT=%s PMSCHECK=%s ", "", "", "NG" );
        return FALSE;
    }

    TMESFilter.Reload(TRet.GetPrintString());
    TPcbList = "";

    if( strcmp(TMESFilter.GetArg((char *)"STATUS"), "PASS") != 0 )
    {
        TRetVal.Format("FRAMELIST=%s PCBCOUNT=%s PMSCHECK=%s ", "", "", "NG" );
        return FALSE;
    }

    TPcbList = GetArgfromMES(TMESFilter, "FRAMELIST");

    CAceStrToken st( TPcbList.GetPrintString(), ",");

    TPcbCount.Format("%d", st.GetCount());

    if ( st.GetCount() > 0 )
    {
        if ( st.GetCount() == 1 )
        {
            if ( TLotID == TPcbList )
            {
                TPMSCheck = "NG";
            }else
            {
                TPMSCheck = "OK";
            }
        }else
        {
            TPMSCheck = "OK";
        }
    }else
    {
        TPMSCheck = "NG";
    }

    TRetVal.Format("FRAMELIST=%s PCBCOUNT=%s PMSCHECK=%s ", TPcbList.GetPrintString(), TPcbCount.GetPrintString(), TPMSCheck.GetPrintString() );

    m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, "    Get_PCBList() :: TRetVal=[%s]", TRetVal.GetPrintString());
    return TRUE;
}

bool CWorkflow::Get_DB_TC_EQP_STATUS(CString TMsg, CString &TRetVal)
{
    CString TRet;

    if( m_TOra.ConnectDB() == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "DB Connect ERROR.");

        TRetVal = "DB Connect ERROR.";
        return TRUE;
    }

    if( m_TOra.Get_EQP_STATUS(TMsg, TRet) == FALSE )
    {
    }

    TRetVal = TRet;

    return TRUE;
}

bool CWorkflow::Set_DB_TC_EQP_STATUS(CString TMsg, CString &TRetVal)
{
    CString TRet;

    if( m_TOra.ConnectDB() == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "DB Connect ERROR.");

        TRetVal = "DB Connect ERROR.";
        return TRUE;
    }

    if( m_TOra.Set_EQP_STATUS(TMsg, TRet) == FALSE )
    {
    }

    TRetVal = TRet;

    return TRUE;
}

bool CWorkflow::MOS_WaferPmsMap(CString &TMsg, CString &TRetVal)
{
    CFilter TFilter(TMsg.GetPrintString());

    CString TBuf, TArg;
    CString TRet;
    CFilter TMESFilter;

    TBuf.Format((char *)"PROCESS=%s LINE=%s EQPID=%s OPERID=%s WAFERID=%s OBJECTID=%s TYPE=%s BASE_WORK_ANGLE=%s CORE_WORK_ANGLE=%s BASE_REVERSE_YN=%s BASE_REVERSE_AXIS=%s CORE_REVERSE_YN=%s CORE_REVERSE_AXIS=%s MES_TARGET=SIMAX", 
            TFilter.GetArg((char *)"PROCESS"), TFilter.GetArg((char *)"LINE"), TFilter.GetArg((char *)"EQPID"), "AUTO",
            TFilter.GetArg((char *)"WAFERID"), TFilter.GetArg((char *)"OBJECTID"), TFilter.GetArg((char *)"TYPE"),
            TFilter.GetArg((char *)"BASE_WORK_ANGLE"), TFilter.GetArg((char *)"CORE_WORK_ANGLE"), TFilter.GetArg((char *)"BASE_REVERSE_YN"), TFilter.GetArg((char *)"BASE_REVERSE_AXIS"), TFilter.GetArg((char *)"CORE_REVERSE_YN"), TFilter.GetArg((char *)"CORE_REVERSE_AXIS"));
    if( MES_Comm("PMSMAP", TBuf, TArg, TArg, TArg, TRet) == FALSE )
    {
        m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "MOS_PmsMap() :: PMSMAP FAIL [%s]", TRet.GetPrintString());
        TRetVal = TRet;
        return FALSE;
    }

    TMESFilter.Reload(TRet.GetPrintString());

    if( strcmp(TMESFilter.GetArg((char *)"STATUS"), "PASS") != 0 )
    {
        TBuf.Format((char *)"%s ", TMESFilter.GetArg("ERRORMSG") );
        TRetVal = TBuf;
        return FALSE;
    }

    TRetVal = "";
    return TRUE;
}

bool CWorkflow::Wafer_PMS_Parsing(CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal)
{
    CFilter         TFilter(cszMsg.GetPrintString());

    FILE            *fp = NULL;
    int             pos1, pos2;
    int             iLoop, nMapDetailIndex;
    char            buf[1024];
    CString         tmpStr, strBuf, strBuf1, strBuf2, strName, strValue;
    CString         TBuf, TRet;

    CString         cszProcName     = TFilter.GetArg((char *)"PROCESS" );
    CString         cszLineID       = TFilter.GetArg((char *)"LINE"    );
    CString         cszLotID        = TFilter.GetArg((char *)"LOTID"   );
    CString         cszObjectID;
    CString         cszWaferID;
    CString         cszEqpID;
    CString         cszDataType;           
    CString         cszMapBaseData;        
    CString         cszMapDetailData;      
    CString         cszMapDetailDataTmp;   

    CAceStrToken    stkValue;              
    CAceStrToken    stkDetailItem;         

    if( (fp = fopen(cszFile.GetPrintString(), "r")) == NULL )
    {
        if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "Wafer_PMS_Parsing : %s Open Error", cszFile.GetPrintString());
        TRetVal = "PMS File :: Open Error";
        return FALSE;
    }

    cszDataType         = "WAFER_MAP_BASE";
    cszMapBaseData      = "";
    cszMapDetailData    = "";
    cszMapDetailDataTmp = "";
    nMapDetailIndex     = 0;

    cszObjectID.Format("PMS%s", GetDateTime_YyyyMmDdHhMiSsCc() );
    TBuf.Format("OBJECT_ID=%s ", cszObjectID.GetPrintString() );
    cszMapBaseData      = TBuf;

    while( !feof(fp) )
    {
        strName     = "";
        strValue    = "";

        memset(buf, 0, sizeof(buf));

        if( fgets(buf, sizeof(buf), fp) == (char *)NULL ) break;

        tmpStr.Empty();
        tmpStr = buf;
        tmpStr.Trim();

        //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Wafer_PMS_Parsing : READ : [%s]", tmpStr.String());

        if( tmpStr.String() != NULL )
        {
            if( tmpStr[0] == '#' ) continue;
            if( tmpStr[0] == '[' ) continue;

            if ( cszDataType == "WAFER_MAP_BASE" )
            {
                pos1 = tmpStr.Find(' ', 0);

                if( pos1 >= 0 )
                {
                    strName = tmpStr.Mid(0, pos1);
                    strName.Trim();

                    if ( tmpStr[tmpStr.Length() - 1] ==  ';' )
                    {
                        strValue = tmpStr.Mid(pos1 + 1, tmpStr.Length() - pos1 - 2);
                        strValue.Trim();
                    }else
                    {
                        strValue = tmpStr.Mid(pos1 + 1, tmpStr.Length() - pos1 - 1);
                        strValue.Trim();
                    }

                    if ( strName == "B_WD" )
                    {
                        cszDataType = "WAFER_MAP_DETAIL";
                        stkDetailItem.Init( tmpStr.String(), " " );
                        continue;
                    }

                    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Wafer_PMS_Parsing : READ : [%s] : NAME=[%s], VALUE=[%s]", cszDataType.GetPrintString(), strName.GetPrintString(), strValue.GetPrintString());
                }else
                {
                    continue;
                }

                strBuf = "";

                if ( strName == "LOTID" )
                {   //LOTID LOTID12;                        LOT_ID
                    //strBuf.Format("LOT_ID=\"%s\" ", strValue.GetPrintString() );
                    if ( cszLotID.Length() > 0 )
                    {
                        strBuf.Format("LOT_ID=\"%s\" ", cszLotID.GetPrintString() );            
                    }else
                    {
                        strBuf.Format("LOT_ID=\"%s\" ", strValue.GetPrintString() );
                    }
                    cszMapBaseData += strBuf;
                }else if ( strName == "STEPID" )
                {   //STEPID A010;                          STEP_SEQ
                    strBuf.Format("STEP_SEQ=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "EQPID" )
                {   //EQPID A123;                           EQP_ID
                    cszEqpID = strValue;
                    strBuf.Format("EQP_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "PARTID" )
                {   //PARTID K4AAAAAAA-AAA;                 PROD_ID
                    strBuf.Format("PROD_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "SLOTID" )
                {   //SLOTID 5;                             SLOT_ID
                    strBuf.Format("SLOT_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "WAFERID" )
                {   //WAFERID AAA111;                       WAFER_ID
                    cszWaferID = strValue;
                    strBuf.Format("WF_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "SAMPLEORIENTATIONMARKTYPE" )
                {   //SAMPLEORIENTATIONMARKTYPE NOTHCH;     SAMPLE_ORIEN_MARK_TYPE
                    strBuf.Format("SAMPLE_ORIEN_MARK_TYPE=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "ORIENTATIONMARKLOCATION" )
                {   //ORIENTATIONMARKLOCATION 0, 90;        ORIEN_MARK_LOC_NOTCH_DEG    ORIEN_MARK_LOC_WORK_DEG
                    //ORIENTATIONMARKLOCATION 0 0;          BASE_WORK_ANGLE             CORE_WORK_ANGLE
                    stkValue.Init( strValue.GetPrintString(), " " );
                    if ( stkValue.GetCount() < 2 )
                    {
                        stkValue.Init( strValue.GetPrintString(), "," );
                    }

                    strBuf1 = stkValue.GetToken(0);
                    strBuf1.Replace( ',', ' ' );
                    strBuf1.Trim();

                    strBuf.Format("ORIEN_MARK_LOC_NOTCH_DEG=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf.Format("BASE_WORK_ANGLE=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf1 = stkValue.GetToken(1);
                    strBuf1.Trim();
                    strBuf.Format("ORIEN_MARK_LOC_WORK_DEG=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf.Format("CORE_WORK_ANGLE=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "DIEPITCH" )
                {   //DIEPITCH 8698.863000 12718.399000;    DIE_PITCH_X_LEN    DIE_PITCH_Y_LEN
                    stkValue.Init( strValue.GetPrintString(), " " );
                    strBuf1 = stkValue.GetToken(0);
                    strBuf1.Trim();

                    strBuf.Format("DIE_PITCH_X_LEN=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf1 = stkValue.GetToken(1);
                    strBuf1.Trim();
                    strBuf.Format("DIE_PITCH_Y_LEN=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "DIEORIGIN" )
                {   //DIEORIGIN 0 0;                        DIE_ORIGIN_X_POSN   DIE_ORIGIN_Y_POSN
                    stkValue.Init( strValue.GetPrintString(), " " );
                    strBuf1 = stkValue.GetToken(0);
                    strBuf1.Trim();

                    strBuf.Format("DIE_ORIGIN_X=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf1 = stkValue.GetToken(1);
                    strBuf1.Trim();
                    strBuf.Format("DIE_ORIGIN_Y=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "WAFER_IN_TIME" )
                {   //WAFER_IN_TIME 2015/01/23_12:46:08;    WAFER_IN_TIME
                    strBuf.Format("WF_IN_TIME=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "WAFER_OUT_TIME" )
                {   //WAFER_OUT_TIME 2015/01/23_12:48:14;   WAFER_OUT_TIME
                    strBuf.Format("WF_OUT_TIME=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "WAFER_TOTAL_COUNT" )
                {   //WAFER_TOTAL_COUNT 750;                WAFER_TOTAL_QTY
                    strBuf.Format("WF_TOTAL_QTY=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "WAFER_GOOD_COUNT" )
                {   //WAFER_GOOD_COUNT 710;                 WAFER_GOOD_QTY
                    strBuf.Format("WF_GOOD_QTY=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BASE_REVERSE_YN" )
                {
                    strBuf.Format("BASE_REVERSE_YN=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BASE_REVERSE_AXIS" )
                {
                    strBuf.Format("BASE_REVERSE_AXIS=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "CORE_REVERSE_YN" )
                {
                    strBuf.Format("CORE_REVERSE_YN=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "CORE_REVERSE_AXIS" )
                {
                    strBuf.Format("CORE_REVERSE_AXIS=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }

            }else if ( cszDataType == "WAFER_MAP_DETAIL" )
            {
                if ( tmpStr[tmpStr.Length() - 1] ==  ';' )
                {
                    strBuf = tmpStr.Mid(0, tmpStr.Length() - 1);
                    strBuf.Trim();
                }else
                {
                    strBuf = tmpStr;
                    strBuf.Trim();
                }

                if ( strBuf == "" || strBuf.Length() < 3 )
                {
                    continue;
                }

                if( strncmp(strBuf.GetPrintString(), "EndOfFile", 9) == 0 || strncmp(strBuf.GetPrintString(), "ENDOFFILE", 9) == 0 )
                {
                    continue;
                }

                //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Wafer_PMS_Parsing : READ : [%s] : Index=[%3d]  VALUE=[%s]", cszDataType.GetPrintString(), nMapDetailIndex, strBuf.GetPrintString());

                stkValue.Init( strBuf.GetPrintString(), " " );
                nMapDetailIndex++;
                strBuf = "";
                strBuf1 = "";

                for( iLoop = 0; iLoop < stkDetailItem.GetCount(); iLoop++ )
                {
                    //MAP_DETAIL_1="aa=3 bb=1 cc=3" MAP_DETAIL_2="aa=3 bb=1 cc=3"

                    //strBuf.Format("MAP_DETAIL_%d=\"%s\" ", nMapDetailIndex, strBuf1.GetPrintString() );
                    //cszMapDetailData += strBuf;

                    strName  = stkDetailItem.GetToken(iLoop);
                    strValue = stkValue.GetToken(iLoop);
                    strBuf2 = "";

                    if ( strName == "B_WD" )
                    {
                        strBuf2.Format("B_WF_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_WX" )
                    {
                        strBuf2.Format("B_WF_X=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_WY" )
                    {
                        strBuf2.Format("B_WF_Y=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_BN" )
                    {
                        strBuf2.Format("B_WF_CELL_BIN=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_GD" )
                    {
                        strBuf2.Format("B_WF_CELL_STATUS=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_WD" )
                    {
                        strBuf2.Format("C_WF_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_WX" )
                    {
                        strBuf2.Format("C_WF_X=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_WY" )
                    {
                        strBuf2.Format("C_WF_Y=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_BN" )
                    {
                        strBuf2.Format("C_WF_CELL_BIN=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "HD" )
                    {
                        strBuf2.Format("HEADER_NO=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "ST" )
                    {
                        strBuf2.Format("CHIP_STACK_CNT=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "PKG2DID" )
                    {
                        strBuf2.Format("CHIP_2D_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }
                }

                strBuf.Format("MAP_DETAIL_%d=\"%s\" ", nMapDetailIndex, strBuf1.GetPrintString() );
                cszMapDetailDataTmp += strBuf;
            }
        }

    }

    strBuf.Format("MAP_DETAIL_LIST_COUNT=%d ", nMapDetailIndex );
    cszMapDetailData += strBuf;
    cszMapDetailData += cszMapDetailDataTmp;

    fclose(fp);


    TMapBaseData    = cszMapBaseData;
    TMapDetailData  = cszMapDetailData;

    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Wafer_PMS_Parsing : Paring : BASE   DATA : [%s]", cszMapBaseData.GetPrintString());
    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Wafer_PMS_Parsing : Paring : DETAIL DATA : [%s]", cszMapDetailData.GetPrintString());

    return TRUE;
}

bool CWorkflow::Boat_PMS_Parsing(CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal)
{
    CFilter         TFilter(cszMsg.GetPrintString());

    FILE            *fp = NULL;
    int             pos1, pos2;
    int             iLoop, nMapDetailIndex;
    char            buf[1024];
    CString         tmpStr, strBuf, strBuf1, strBuf2, strName, strValue;
    CString         TBuf, TRet;

    CString         cszProcName     = TFilter.GetArg((char *)"PROCESS" );
    CString         cszLineID       = TFilter.GetArg((char *)"LINE"    );
    CString         cszLotID        = TFilter.GetArg((char *)"LOTID"   );
    CString         cszObjectID;
    CString         cszEqpID;
    CString         cszDataType;            
    CString         cszMapBaseData;         
    CString         cszMapDetailData;       
    CString         cszMapDetailDataTmp;    

    CAceStrToken    stkValue;               
    CAceStrToken    stkDetailItem;          

    if( (fp = fopen(cszFile.GetPrintString(), "r")) == NULL )
    {
        if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "Boat_PMS_Parsing : %s Open Error", cszFile.GetPrintString());
        TRetVal = "PMS File :: Open Error";
        return FALSE;
    }

    cszDataType         = "BOAT_MAP_BASE";
    cszMapBaseData      = "";
    cszMapDetailData    = "";
    cszMapDetailDataTmp = "";
    nMapDetailIndex     = 0;

    cszObjectID.Format("PMS%s", GetDateTime_YyyyMmDdHhMiSsCc() );
    TBuf.Format("OBJECT_ID=%s ", cszObjectID.GetPrintString() );
    cszMapBaseData      = TBuf;

    while( !feof(fp) )
    {
        strName     = "";
        strValue    = "";

        memset(buf, 0, sizeof(buf));

        if( fgets(buf, sizeof(buf), fp) == (char *)NULL ) break;

        tmpStr.Empty();
        tmpStr = buf;
        tmpStr.Trim();

        //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Boat_PMS_Parsing : READ : [%s]", tmpStr.String());

        if( tmpStr.String() != NULL )
        {
            if( tmpStr[0] == '#' ) continue;
            if( tmpStr[0] == '[' ) continue;

            if ( cszDataType == "BOAT_MAP_BASE" )
            {
                pos1 = tmpStr.Find(' ', 0);

                if( pos1 >= 0 )
                {
                    strName = tmpStr.Mid(0, pos1);
                    strName.Trim();

                    if ( tmpStr[tmpStr.Length() - 1] ==  ';' )
                    {
                        strValue = tmpStr.Mid(pos1 + 1, tmpStr.Length() - pos1 - 2);
                        strValue.Trim();
                    }else
                    {
                        strValue = tmpStr.Mid(pos1 + 1, tmpStr.Length() - pos1 - 1);
                        strValue.Trim();
                    }

                    if ( strName == "B_BD" )
                    {
                        cszDataType = "BOAT_MAP_DETAIL";
                        stkDetailItem.Init( tmpStr.String(), " " );
                        continue;
                    }

                    if ( strName == "T_BD" )
                    {
                        cszDataType = "BOAT_MAP_DETAIL";
                        stkDetailItem.Init( tmpStr.String(), " " );
                        continue;
                    }

                    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Boat_PMS_Parsing : READ : [%s] : NAME=[%s], VALUE=[%s]", cszDataType.GetPrintString(), strName.GetPrintString(), strValue.GetPrintString());
                }else
                {
                    continue;
                }

                strBuf = "";

                if ( strName == "LOTID" )
                {   //LOTID LOTID12;                        LOT_ID
                    //strBuf.Format("LOT_ID=\"%s\" ", strValue.GetPrintString() );
                    if ( cszLotID.Length() > 0 )
                    {
                        strBuf.Format("LOT_ID=\"%s\" ", cszLotID.GetPrintString() );            
                    }else
                    {
                        strBuf.Format("LOT_ID=\"%s\" ", strValue.GetPrintString() );
                    }
                    cszMapBaseData += strBuf;
                }else if ( strName == "STEPID" )
                {   //STEPID A010;                          STEP_SEQ
                    strBuf.Format("STEP_SEQ=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "EQPID" )
                {   //EQPID A123;                           EQP_ID
                    cszEqpID = strValue;
                    strBuf.Format("EQP_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "PARTID" )
                {   //PARTID K4AAAAAAA-AAA;                 PROD_ID
                    strBuf.Format("PROD_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "SLOTID" )
                {   //SLOTID 5;                             SLOT_ID
                    strBuf.Format("SLOT_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BOATID" )
                {   //BOATID AAA111;                        BOAT_ID
                    strBuf.Format("BOAT_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "FLIP_BOATID" )
                {   //FLIP_BOATID BBB111;                   FLIP_BOAT_ID
                    strBuf.Format("FLIP_BOAT_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "ORIENTATIONMARKLOCATION" )
                {   //ORIENTATIONMARKLOCATION 0 90;         ORIEN_MARK_LOC_BOAT    ORIEN_MARK_LOC_WAFER
                    stkValue.Init( strValue.GetPrintString(), " " );

                    strBuf1 = stkValue.GetToken(0);
                    strBuf1.Trim();

                    strBuf.Format("ORIEN_MARK_LOC_BOAT=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf1 = stkValue.GetToken(1);
                    strBuf1.Trim();
                    strBuf.Format("ORIEN_MARK_LOC_WAFER=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BOAT_ARRAY" )
                {   //BOAT_ARRAY 5,2;                       BOAT_ARRAY_X    BOAT_ARRAY_Y
                    stkValue.Init( strValue.GetPrintString(), " " );
                    if ( stkValue.GetCount() < 2 )
                    {
                        stkValue.Init( strValue.GetPrintString(), "," );
                    }

                    strBuf1 = stkValue.GetToken(0);
                    strBuf1.Replace( ',', ' ' );
                    strBuf1.Trim();

                    strBuf.Format("BOAT_ARRAY_X=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf1 = stkValue.GetToken(1);
                    strBuf1.Trim();
                    strBuf.Format("BOAT_ARRAY_Y=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "TRAY_ARRAY" )
                {   //TRAY_ARRAY 5,2;                       TRAY_ARRAY_X    TRAY_ARRAY_Y
                    stkValue.Init( strValue.GetPrintString(), " " );
                    if ( stkValue.GetCount() < 2 )
                    {
                        stkValue.Init( strValue.GetPrintString(), "," );
                    }

                    strBuf1 = stkValue.GetToken(0);
                    strBuf1.Replace( ',', ' ' );
                    strBuf1.Trim();

                    strBuf.Format("TRAY_ARRAY_X=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;

                    strBuf1 = stkValue.GetToken(1);
                    strBuf1.Trim();
                    strBuf.Format("TRAY_ARRAY_Y=\"%s\" ", strBuf1.GetPrintString() );
                    cszMapBaseData += strBuf;
                }
                else if ( strName == "BOAT_WORK_TYPE" )
                {   //BOAT_WORK_TYPE 1;                     BOAT_WORK_TYPE
                    strBuf.Format("BOAT_WORK_TYPE=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BOAT_IN_TIME" )
                {   //BOAT_IN_TIME 2015/01/23_12:48:14;     BOAT_IN_TIME
                    strBuf.Format("BOAT_IN_TIME=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "TRAY_IN_TIME" )
                {   //TRAY_IN_TIME 2015/01/23_12:48:14;     TRAY_IN_TIME
                    strBuf.Format("TRAY_IN_TIME=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BOAT_OUT_TIME" )
                {   //BOAT_OUT_TIME 2015/01/23_12:48:14;    BOAT_OUT_TIME
                    strBuf.Format("BOAT_OUT_TIME=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "TRAY_OUT_TIME" )
                {   //TRAY_OUT_TIME 2015/01/23_12:48:14;    TRAY_OUT_TIME
                    strBuf.Format("TRAY_OUT_TIME=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BOAT_TOTAL_COUNT" )
                {   //BOAT_TOTAL_COUNT 12;                  BOAT_TOTAL_COUNT
                    strBuf.Format("BOAT_TOTAL_COUNT=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "TRAY_TOTAL_COUNT" )
                {   //TRAY_TOTAL_COUNT 12;                  TRAY_TOTAL_COUNT
                    strBuf.Format("TRAY_TOTAL_COUNT=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "BOAT_GOOD_COUNT" )
                {   //BOAT_GOOD_COUNT 12;                   BOAT_GOOD_COUNT
                    strBuf.Format("BOAT_GOOD_COUNT=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }

            }else if ( cszDataType == "BOAT_MAP_DETAIL" )
            {
                if ( tmpStr[tmpStr.Length() - 1] ==  ';' )
                {
                    strBuf = tmpStr.Mid(0, tmpStr.Length() - 1);
                    strBuf.Trim();
                }else
                {
                    strBuf = tmpStr;
                    strBuf.Trim();
                }

                if ( strBuf == "" || strBuf.Length() < 3 )
                {
                    continue;
                }

                if( strncmp(strBuf.GetPrintString(), "EndOfFile", 9) == 0 || strncmp(strBuf.GetPrintString(), "ENDOFFILE", 9) == 0 )
                {
                    continue;
                }

                //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Boat_PMS_Parsing : READ : [%s] : Index=[%3d]  VALUE=[%s]", cszDataType.GetPrintString(), nMapDetailIndex, strBuf.GetPrintString());

                stkValue.Init( strBuf.GetPrintString(), " " );
                nMapDetailIndex++;
                strBuf = "";
                strBuf1 = "";

                for( iLoop = 0; iLoop < stkDetailItem.GetCount(); iLoop++ )
                {
                    //MAP_DETAIL_1="aa=3 bb=1 cc=3" MAP_DETAIL_2="aa=3 bb=1 cc=3"

                    //strBuf.Format("MAP_DETAIL_%d=\"%s\" ", nMapDetailIndex, strBuf1.GetPrintString() );
                    //cszMapDetailData += strBuf;

                    strName  = stkDetailItem.GetToken(iLoop);
                    strValue = stkValue.GetToken(iLoop);
                    strBuf2 = "";

                    if ( strName == "B_BD" )
                    {
                        strBuf2.Format("B_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "T_BD" )
                    {
                        strBuf2.Format("B_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_BX" )
                    {
                        strBuf2.Format("B_X=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "T_BX" )
                    {
                        strBuf2.Format("B_X=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_BY" )
                    {
                        strBuf2.Format("B_Y=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "T_BY" )
                    {
                        strBuf2.Format("B_Y=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_BN" )
                    {
                        strBuf2.Format("B_CELL_BIN=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "T_BN" )
                    {
                        strBuf2.Format("B_CELL_BIN=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_GD" )
                    {
                        strBuf2.Format("B_CELL_STATUS=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "T_GD" )
                    {
                        strBuf2.Format("B_CELL_STATUS=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_BD" )
                    {
                        strBuf2.Format("C_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "H_BD" )
                    {
                        strBuf2.Format("C_ID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_BX" )
                    {
                        strBuf2.Format("C_X=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "H_BX" )
                    {
                        strBuf2.Format("C_X=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_BY" )
                    {
                        strBuf2.Format("C_Y=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "H_BY" )
                    {
                        strBuf2.Format("C_Y=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "C_BN" )
                    {
                        strBuf2.Format("C_CELL_BIN=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "H_BN" )
                    {
                        strBuf2.Format("C_CELL_BIN=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "HD" )
                    {
                        strBuf2.Format("HEADER_NO=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "ST" )
                    {
                        strBuf2.Format("CHIP_STACK_CNT=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }
                }

                strBuf.Format("MAP_DETAIL_%d=\"%s\" ", nMapDetailIndex, strBuf1.GetPrintString() );
                cszMapDetailDataTmp += strBuf;
            }
        }

    }

    strBuf.Format("MAP_DETAIL_LIST_COUNT=%d ", nMapDetailIndex );
    cszMapDetailData += strBuf;
    cszMapDetailData += cszMapDetailDataTmp;

    fclose(fp);


    TMapBaseData    = cszMapBaseData;
    TMapDetailData  = cszMapDetailData;

    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Boat_PMS_Parsing : Paring : BASE   DATA : [%s]", cszMapBaseData.GetPrintString());
    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Boat_PMS_Parsing : Paring : DETAIL DATA : [%s]", cszMapDetailData.GetPrintString());

    return TRUE;
}

bool CWorkflow::Reel_PMS_Parsing(CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal)
{
    CFilter         TFilter(cszMsg.GetPrintString());
    CFilter         TMESFilter;

    FILE            *fp = NULL;
    int             pos1, pos2;
    int             iLoop, nMapDetailIndex;
    char            buf[1024];
    CString         tmpStr, strBuf, strBuf1, strBuf2, strName, strValue;
    CString         TBuf, TRet, TArg;

    CString         cszProcName     = TFilter.GetArg((char *)"PROCESS" );
    CString         cszLineID       = TFilter.GetArg((char *)"LINE"    );
    CString         cszLotID;
    CString         cszObjectID;
    CString         cszEqpID;
    CString         cszDataType;            
    CString         cszMapBaseData;         
    CString         cszMapDetailData;       
    CString         cszMapDetailDataTmp;    

    CAceStrToken    stkValue;               
    CAceStrToken    stkDetailItem;          

    if( (fp = fopen(cszFile.GetPrintString(), "r")) == NULL )
    {
        if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB2, "Reel_PMS_Parsing : %s Open Error", cszFile.GetPrintString());
        TRetVal = "PMS File :: Open Error";
        return FALSE;
    }

    cszDataType         = "MAP_BASE";
    cszMapBaseData      = "";
    cszMapDetailData    = "";
    cszMapDetailDataTmp = "";
    nMapDetailIndex     = 0;

    cszObjectID.Format("PMS%s", GetDateTime_YyyyMmDdHhMiSsCc() );
    TBuf.Format("OBJECT_ID=%s ", cszObjectID.GetPrintString() );
    cszMapBaseData      = TBuf;

    while( !feof(fp) )
    {
        strName     = "";
        strValue    = "";

        memset(buf, 0, sizeof(buf));

        if( fgets(buf, sizeof(buf), fp) == (char *)NULL ) break;

        tmpStr.Empty();
        tmpStr = buf;
        tmpStr.Trim();

        //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Reel_PMS_Parsing : READ : [%s]", tmpStr.String());

        if( tmpStr.String() != NULL )
        {
            if( tmpStr[0] == '#' ) continue;
            if( tmpStr[0] == '[' )
            {
                char *ptr = strstr(tmpStr.String(), "[TR_REEL_REPORT]");
                if( ptr )
                {
                    cszDataType         = "MAP_BASE";
                }

                ptr = strstr(tmpStr.String(), "[DEVICE_TO_REEL]");
                if( ptr )
                {
                    cszDataType         = "MAP_DETAIL";
                }

                continue;
            }

            if ( cszDataType == "MAP_BASE" )
            {
                pos1 = tmpStr.Find(' ', 0);

                if( pos1 >= 0 )
                {
                    strName = tmpStr.Mid(0, pos1);
                    strName.Trim();

                    if ( tmpStr[tmpStr.Length() - 1] ==  ';' )
                    {
                        strValue = tmpStr.Mid(pos1 + 1, tmpStr.Length() - pos1 - 2);
                        strValue.Trim();
                    }else
                    {
                        strValue = tmpStr.Mid(pos1 + 1, tmpStr.Length() - pos1 - 1);
                        strValue.Trim();
                    }
                    //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Reel_PMS_Parsing : READ : [%s] : NAME=[%s], VALUE=[%s]", cszDataType.GetPrintString(), strName.GetPrintString(), strValue.GetPrintString());
                }else
                {
                    continue;
                }

                strBuf = "";

                if ( strName == "EQPID" )
                {   //EQPID A123;                           EQP_ID
                    cszEqpID = strValue;
                    strBuf.Format("EQP_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "STEPID" )
                {   //STEPID A010;                          STEP_SEQ
                    strBuf.Format("STEP_SEQ=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "LOTID" )
                {   //LOTID LOTID12;                        LOT_ID
                    cszLotID = strValue;
                    strBuf.Format("LOT_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "PARENTLOTID" )
                {   //PARENTLOTID GPB00938;                 PARENT_LOT_ID
                    strBuf.Format("PARENT_LOT_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "PARTID" )
                {   //PARTID K4AAAAAAA-AAA;                 PROD_ID
                    strBuf.Format("PROD_ID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "JOBID" )
                {   //JOBID NOR_4942MPGA_7.975X11.975_8X22_N_0.78_2D;           JOBID
                    strBuf.Format("JOBID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "TRAYID" )
                {   //TRAYID ;                              TRAYID
                    strBuf.Format("TRAYID=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "PACKAGE" )
                {   //PACKAGE NOR_4942MPGA_7.975X11.975_8X22_N_0.78_2D;         PACKAGE
                    strBuf.Format("PACKAGE=\"%s\" ", strValue.GetPrintString() );
                    cszMapBaseData += strBuf;
                }else if ( strName == "START_TIME" )
                {   //START_TIME 2016-07-15 20:28:45;       START_TIME
                    stkValue.Init( strValue.GetPrintString(), " " );

                    strBuf.Format("START_TIME=\"%s %s\" ", stkValue.GetToken(0), stkValue.GetToken(1) );
                    cszMapBaseData += strBuf;
                }else if ( strName == "END_TIME" )
                {   //END_TIME 2016-07-15 20:28:45;       END_TIME
                    stkValue.Init( strValue.GetPrintString(), " " );

                    strBuf.Format("END_TIME=\"%s %s\" ", stkValue.GetToken(0), stkValue.GetToken(1) );
                    cszMapBaseData += strBuf;
                }

            }else if ( cszDataType == "MAP_DETAIL" )
            {
                char *ptr = strstr(tmpStr.String(), "NO REEL MODE");
                if( ptr )
                {
                    stkDetailItem.Init( tmpStr.String(), " " );         
                    continue;
                }

                if ( tmpStr[tmpStr.Length() - 1] ==  ';' )
                {
                    strBuf = tmpStr.Mid(0, tmpStr.Length() - 1);
                    strBuf.Trim();
                }else
                {
                    strBuf = tmpStr;
                    strBuf.Trim();
                }

                if ( strBuf == "" || strBuf.Length() < 3 )
                {
                    continue;
                }

                if( strncmp(strBuf.GetPrintString(), "EndOfFile", 9) == 0 || strncmp(strBuf.GetPrintString(), "ENDOFFILE", 9) == 0 || strncmp(strBuf.GetPrintString(), "ENDOFFLINE", 10) == 0 )
                {
                    continue;
                }

                //if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Reel_PMS_Parsing : READ : [%s] : Index=[%3d]  VALUE=[%s]", cszDataType.GetPrintString(), nMapDetailIndex, strBuf.GetPrintString());

                stkValue.Init( strBuf.GetPrintString(), " " );
                nMapDetailIndex++;
                strBuf = "";
                strBuf1 = "";

                for( iLoop = 0; iLoop < stkDetailItem.GetCount(); iLoop++ )
                {
                    //MAP_DETAIL_1="aa=3 bb=1 cc=3" MAP_DETAIL_2="aa=3 bb=1 cc=3"

                    //strBuf.Format("MAP_DETAIL_%d=\"%s\" ", nMapDetailIndex, strBuf1.GetPrintString() );
                    //cszMapDetailData += strBuf;

                    strName  = stkDetailItem.GetToken(iLoop);
                    strValue = stkValue.GetToken(iLoop);
                    strBuf2 = "";

                    if ( strName == "NO" )
                    {
                        strBuf2.Format("NO=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "REEL" )
                    {
                        strBuf2.Format("REEL=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "MODE" )
                    {
                        strBuf2.Format("MODE=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_WD" )      
                    {
                        strBuf2.Format("B_WD=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_WX" )      
                    {
                        strBuf2.Format("B_WX=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "B_WY" )      
                    {
                        strBuf2.Format("B_WY=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "UPI" )
                    {
                        strBuf2.Format("UPI=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "PKGID" )
                    {
                        strBuf2.Format("PKGID=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "DATE" )
                    {
                        strBuf2.Format("DATE=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }else if ( strName == "TIME" )
                    {
                        strBuf2.Format("TIME=%s ", strValue.GetPrintString() );
                        strBuf1 += strBuf2;
                    }
                }

                strBuf.Format("MAP_DETAIL_%d=\"%s\" ", nMapDetailIndex, strBuf1.GetPrintString() );
                cszMapDetailDataTmp += strBuf;
            }
        }
    }

    strBuf.Format("MAP_DETAIL_LIST_COUNT=%d ", nMapDetailIndex );
    cszMapDetailData += strBuf;
    cszMapDetailData += cszMapDetailDataTmp;

    fclose(fp);

    TMapBaseData    = cszMapBaseData;
    TMapDetailData  = cszMapDetailData;

    if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Reel_PMS_Parsing : Paring : BASE   DATA : [%s]", cszMapBaseData.GetPrintString());
    if( m_pTLog ) m_pTLog->WriteTC(eLOG_TYPE_DEBUG, eLOG_IFS_NONE, eTAB2, "Reel_PMS_Parsing : Paring : DETAIL DATA : [%s]", cszMapDetailData.GetPrintString());

    return TRUE;
}

bool CWorkflow::GetTrackingMsg(CString TCmd, CString TData, CString &TMsg, CString &TErrMsg, CString &TMesTarget /*= "-" */)
{
    CString TBuf;
    CString TMesTargetData;
    CFilter TFilter( TData.GetPrintString() );


    if( CheckPara(TFilter, "PROCESS" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
    //if( CheckPara(TFilter, "LINE"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
    if( CheckPara(TFilter, "EQPID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

    TMesTargetData = TFilter.GetArg((char *)"MESTARGET");

    if( TCmd == "MODULEEQPSTAT" )
    {
        if( CheckPara(TFilter, "MODE" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CODE" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MODULEEQPSTAT HDR=(LOTmgr,%s,MODULEEQPSTAT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "MODE=%s CODE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"MODE"),
                TFilter.GetArg((char *)"CODE") );

        AddMsg( TFilter, TMsg, "EQPSTATUS"  , "EQPSTATUS"           );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT", "\"", "\"" );
        AddMsg( TFilter, TMsg, "ENABLEOK"   , "ENABLEOK"            );
        AddMsg( TFilter, TMsg, "SUBMODE"    , "SUBMODE"             );
        AddMsg( TFilter, TMsg, "NOTE"       , "NOTE"                );
        AddMsg( TFilter, TMsg, "TYPE"       , "TYPE"                );
        AddMsg( TFilter, TMsg, "CHAMBERCTL" , "CHAMBERCTL"          );
        AddMsg( TFilter, TMsg, "SOURCE"     , "SOURCE"              );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"            );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CHANGEMAGAZINEID" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CARRIERID"  , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHANGEMAGAZINEID HDR=(LOTmgr,%s,CHANGEMAGAZINEID) LINEID=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CARRIERID=%s LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CARRIERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "ACTIONTYPE"     , "ACTIONTYPE" );
        AddMsg( TFilter, TMsg, "OLD_CARRIERID"  , "OLD_CARRIERID" );
        AddMsg( TFilter, TMsg, "CARR_TYPE"      , "CARR_TYPE" );
        AddMsg( TFilter, TMsg, "SUB_CARR_TYPE"  , "SUB_CARR_TYPE" );
        AddMsg( TFilter, TMsg, "SUB_CARR_QTY"   , "SUB_CARR_QTY" );
        AddMsg( TFilter, TMsg, "COMP_QTY"       , "COMP_QTY" );
        AddMsg( TFilter, TMsg, "CALLTYPE"       , "CALLTYPE" );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES" );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT", "\"", "\"" );
        AddMsg( TFilter, TMsg, "REQ_SYS"        , "REQ_SYS" );
        AddMsg( TFilter, TMsg, "MAGAZINEID"     , "MAGAZINEID" ); 

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CHKMERGE" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHKMERGE HDR=(LOTmgr,%s,CHKMERGE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SUBLOTID"   , "SUBLOTID", "(", ")" );
        AddMsg( TFilter, TMsg, "SBLFLAG"    , "SBLFLAG" );
        AddMsg( TFilter, TMsg, "FRMCONV"    , "FRMCONV" );
        AddMsg( TFilter, TMsg, "REQ_SYS"    , "REQ_SYS" );
        AddMsg( TFilter, TMsg, "BOAT_FLAG"  , "BOAT_FLAG" );  
        AddMsg( TFilter, TMsg, "SPLIT_OPTION"   , "SPLIT_OPTION"        );  

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CREATESTART" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "QUANTITY"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "LOCATION"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "LOTTYPE"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "PRODUCT"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "VENDORID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CREATESTART HDR=(LOTmgr,%s,CREATESTART) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s QUANTITY=%s LOCATION=%s LOTTYPE=%s PRODUCT=%s VENDORID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"QUANTITY"),
                TFilter.GetArg((char *)"LOCATION"),
                TFilter.GetArg((char *)"LOTTYPE"),
                TFilter.GetArg((char *)"PRODUCT"),
                TFilter.GetArg((char *)"VENDORID") );

        AddMsg( TFilter, TMsg, "PROCESSID"      , "PROCESSID"                   );
        AddMsg( TFilter, TMsg, "PRIORITY"       , "PRIORITY"                    );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "INGOTID"        , "INGOTID"                     );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "ORDERNUM"       , "ORDERNUM"                    );
        AddMsg( TFilter, TMsg, "DUEDATE"        , "DUEDATE"                     );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"                     );
        AddMsg( TFilter, TMsg, "UNIT"           , "UNIT"                        );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "VENDORLOTID"    , "VENDORLOTID"                 );
        AddMsg( TFilter, TMsg, "SOURCELOTID"    , "SOURCELOTID"                 );
        AddMsg( TFilter, TMsg, "CONSUMEINFO"    , "CONSUMEINFO"                 );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "FRAMEDISPLAY" )
    {
        if( CheckPara(TFilter, "FRAMEID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"FRAMEDISPLAY HDR=(LOTmgr,%s,FRAMEDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "FRAMEID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"FRAMEID") );

        AddMsg( TFilter, TMsg, "STEPDESC"   , "STEPDESC"    );
        AddMsg( TFilter, TMsg, "PKGID_CHK"  , "PKGID_CHK"   );
        AddMsg( TFilter, TMsg, "INFO_TYPE"  , "INFO_TYPE"   );
        AddMsg( TFilter, TMsg, "CHIPIDINFO" , "CHIPIDINFO"  );
        AddMsg( TFilter, TMsg, "PMSFLAG"    , "PMSFLAG"     );  
        AddMsg( TFilter, TMsg, "MULTICHIPFLAG", "MULTICHIPFLAG" );  

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "FRAMEMAP" )
    {
        if( CheckPara(TFilter, "FRAMEID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ARRAY_X"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ARRAY_Y"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "MAPINFO"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"FRAMEMAP HDR=(LOTmgr,%s,FRAMEMAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "FRAMEID=%s ARRAY_X=%s ARRAY_Y=%s MAPINFO=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"  ),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"FRAMEID"),
                TFilter.GetArg((char *)"ARRAY_X"),
                TFilter.GetArg((char *)"ARRAY_Y"),
                TFilter.GetArg((char *)"MAPINFO") );

        AddMsg( TFilter, TMsg, "LOTID"      , "LOTID"       );
        AddMsg( TFilter, TMsg, "PKGID"      , "PKGID"       );
        AddMsg( TFilter, TMsg, "MAPTYPE"    , "MAPTYPE"     );
        AddMsg( TFilter, TMsg, "MAPUNIT"    , "MAPUNIT"     );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"    );
        AddMsg( TFilter, TMsg, "FRAME_DUP_CHK", "FRAME_DUP_CHK"  ); 
        AddMsg( TFilter, TMsg, "CHIPIDINFO" , "CHIPIDINFO" , "(", ")"  );
        AddMsg( TFilter, TMsg, "PMSFLAG"    , "PMSFLAG"     );  
        AddMsg( TFilter, TMsg, "FRAMELIST"    , "FRAMELIST",    "(",    ")"     );      

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "FRAMESPLIT" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "FRAMEID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SPLITINFO"  , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"FRAMESPLIT HDR=(LOTmgr,%s,FRAMESPLIT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s FRAMEID=%s SPLITINFO=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"FRAMEID"),
                TFilter.GetArg((char *)"SPLITINFO") );

        AddMsg( TFilter, TMsg, "ARRAY_X"    , "ARRAY_X"     );
        AddMsg( TFilter, TMsg, "ARRAY_Y"    , "ARRAY_Y"     );
        AddMsg( TFilter, TMsg, "MAPINFO"    , "MAPINFO"     );
        AddMsg( TFilter, TMsg, "PKGID"      , "PKGID"       );
        AddMsg( TFilter, TMsg, "SPLITTYPE"  , "SPLITTYPE"   );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "FRAMEDELETE" )
    {
        if( CheckPara(TFilter, "FRAMEID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"FRAMEDELETE HDR=(LOTmgr,%s,FRAMEDELETE) OPERID=%s REQ_SYSTEM=TC "
                            "FRAMEID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"FRAMEID") );

        AddMsg( TFilter, TMsg, "LOT_MAPPING"    , "LOT_MAPPING"     );

        TMesTarget = "SIMAX";
    }else if( TCmd == "CHIP_SORT_RESULT" )    
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ISSUE_NO"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        //CHIP_SORT_RESULT HDR=(IMSmgr,LH.MODULE11030,CHIP_SORT_RESULT) LOTID=Y0ZW250743 OPERID=06345171 EQPID=AVI ISSUE_NO=ISSUE-001 CHIP_DATA_TYPE=SSD_P_SRL CHIP_DATA=(S2XVNY1JA32963,S2XVNY1JA32766,S2XVNY1JA32633,S2XVNY1JA32691,S2XVNY1JA32948,S2XVNY1JA32700,S2XVNY1JA32565,S2XVNY1JA32560,S2XVNY1JA32947,S2XVNY1JA32559,S2XVNY1JA32914) REQ_SYSTEM=TC
        //CHIP_SORT_RESULT_REP HDR=(IMSmgr,LH.MODULE11030,CHIP_SORT_RESULT) STATUS=PASS LOTID=Y0ZW250743
        TMsg.Format((char *)"CHIP_SORT_RESULT HDR=(LOTmgr,%s,CHIP_SORT_RESULT) LOTID=%s OPERID=%s EQPID=%s ISSUE_NO=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"ISSUE_NO")
            );

        AddMsg( TFilter, TMsg, "CHIP_DATA_TYPE"     , "CHIP_DATA_TYPE"                      );
        AddMsg( TFilter, TMsg, "CHIP_DATA"          , "CHIP_DATA",          "(",    ")"     );

        TMesTarget = "SIMAX";
    }else if( TCmd == "GET_SPLIT_LOTID" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SPLITTYPE"  , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"GET_SPLIT_LOTID HDR=(LOTmgr,%s,GET_SPLIT_LOTID) LINEID=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SPLITTYPE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SPLITTYPE") );

        AddMsg( TFilter, TMsg, "BININFO"    , "BININFO"     );
        AddMsg( TFilter, TMsg, "WAFERNO"    , "WAFERNO"     );
        AddMsg( TFilter, TMsg, "SPLITGUBUN" , "SPLITGUBUN"  );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "HOLD" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "COMMENT"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CODE"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"HOLD HDR=(LOTmgr,%s,HOLD) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s COMMENT=\"%s\" CODE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"COMMENT"),
                TFilter.GetArg((char *)"CODE") );

        AddMsg( TFilter, TMsg, "MULTIFLAG"      , "MULTIFLAG"                       );
        AddMsg( TFilter, TMsg, "HOLDTYPE"       , "HOLDTYPE"                        );
        AddMsg( TFilter, TMsg, "FUTURERECIPEID" , "FUTURERECIPEID"                  );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "ISSUE" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"ISSUE HDR=(LOTmgr,%s,ISSUE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "NEWLOTID"       , "NEWLOTID"                        );
        AddMsg( TFilter, TMsg, "NEWPARTID"      , "NEWPARTID"                       );
        AddMsg( TFilter, TMsg, "LOCATION"       , "LOCATION"                        );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"                         );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "NEWPROCESSID"   , "NEWPROCESSID"                    );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                        );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "SPLITWFINFO"    , "SPLITWFINFO"                     );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTCARD" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STEPSEQ"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTCARD HDR=(LOTmgr,%s,LOTCARD) LINEID=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s STEPSEQ=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"STEPSEQ") );

        AddMsg( TFilter, TMsg, "SCRAPINFO", "SCRAPINFO", "(", ")" );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTCLOSECHK" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTCLOSECHK HDR=(LOTmgr,%s,LOTCLOSECHK) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );
         AddMsg( TFilter, TMsg, "MORDER_LIST", "MORDER_LIST", "(", ")" );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTCOMMENT" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "COMMENT"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTCOMMENT HDR=(LOTmgr,%s,LOTCOMMENT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s COMMENT=\"%s\" ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"COMMENT") );

        AddMsg( TFilter, TMsg, "METALINFO"  , "METALINFO"   );
        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"    );
        AddMsg( TFilter, TMsg, "STEPID"     , "STEPID"      );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"    );
        AddMsg( TFilter, TMsg, "TXNTIME"    , "TXNTIME"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTCONSUMEMATERIAL" )
    {
        //if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTCONSUMEMATERIAL HDR=(LOTmgr,%s,LOTCONSUMEMATERIAL) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID") );

        AddMsg( TFilter, TMsg, "LOTID"          , "LOTID"                           );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                             );
        AddMsg( TFilter, TMsg, "TARGETLOT"      , "TARGETLOT"                       );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "COMPONENTS"     , "COMPONENTS"                      );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "CONSUMERLOTID"  , "CONSUMERLOTID"                   );//2023.06.07 AJS : Add
        AddMsg( TFilter, TMsg, "CONSUMELOTINFO" , "CONSUMELOTINFO" ,"(" ,    ")"    );//2023.06.07 AJS : Add
        AddMsg( TFilter, TMsg, "TERMINATED_YN"  , "TERMINATED_YN"                   );//2023.11.17 AJS : Modify

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTDISPLAY" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTDISPLAY HDR=(LOTmgr,%s,LOTDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "ATTRIBUTE"      , "ATTRIBUTE"       );
        AddMsg( TFilter, TMsg, "NEXTSTPS"       , "NEXTSTPS"        );
        AddMsg( TFilter, TMsg, "MCPINFO"        , "MCPINFO"         );
        AddMsg( TFilter, TMsg, "BAKEREMAINTIME" , "BAKEREMAINTIME"  );
        AddMsg( TFilter, TMsg, "STEPSKIPINFO"   , "STEPSKIPINFO"    );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"       );
        AddMsg( TFilter, TMsg, "PLASMAFLAG"     , "PLASMAFLAG"      );
        AddMsg( TFilter, TMsg, "TRAY_INFO"      , "TRAY_INFO"       );
        AddMsg( TFilter, TMsg, "REQTYPE"        , "REQTYPE"         );
        AddMsg( TFilter, TMsg, "WAFER_INFO_YN"  , "WAFER_INFO_YN"   );
        AddMsg( TFilter, TMsg, "MTPCODE"        , "MTPCODE"         );
        AddMsg( TFilter, TMsg, "MODE"           , "MODE"            );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTIDCHG" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "NEWLOTID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTIDCHG HDR=(LOTmgr,%s,LOTIDCHG) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s NEWLOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"NEWLOTID") );

        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"                    );
        AddMsg( TFilter, TMsg, "TXNTIME"    , "TXNTIME"                     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TKIN" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TKIN HDR=(LOTmgr,%s,TKIN) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "EQPTYPE"        , "EQPTYPE"                         );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"     ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "LINE"           , "LINE"                            );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                             );
        AddMsg( TFilter, TMsg, "BATCHID"        , "BATCHID"                         );
        AddMsg( TFilter, TMsg, "MERGESTEP"      , "MERGESTEP"                       );
        AddMsg( TFilter, TMsg, "MULTIEQPINFO"   , "MULTIEQPINFO",   "(",    ")"     );
        AddMsg( TFilter, TMsg, "ATTRINFO"       , "ATTRINFO"    ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR"        ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "EQPSTATUS"      , "EQPSTATUS"                       );
        AddMsg( TFilter, TMsg, "SOCKETINFO"     , "SOCKETINFO"  ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "MATINFO"        , "MATINFO"     ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "REQTYPE"        , "REQTYPE"                         );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        ); 
        AddMsg( TFilter, TMsg, "FRMCONV"        , "FRMCONV"                         );
        AddMsg( TFilter, TMsg, "BOAT_FLAG"      , "BOAT_FLAG"                       );
            AddMsg( TFilter, TMsg, "FIRST_PCB"      , "FIRST_PCB"                       );
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTMATCHG" )
    {
        TMsg.Format((char *)"LOTMATCHG HDR=(LOTmgr,%s,LOTMATCHG) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID") );

        AddMsg( TFilter, TMsg, "LINE"           , "LINEID"                              );
        AddMsg( TFilter, TMsg, "PARTID"         , "PARTID"                              );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"                             );
        AddMsg( TFilter, TMsg, "MATINFO"        , "MATINFO",            "(",    ")"     );
        AddMsg( TFilter, TMsg, "CURMATLOTID"    , "CURMATLOTID"                         );
        AddMsg( TFilter, TMsg, "MATLOTID"       , "MATLOTID"                            );
        AddMsg( TFilter, TMsg, "REASON"         , "REASON"                              );
        AddMsg( TFilter, TMsg, "MAT_VENDOR"     , "MAT_VENDOR"                          );
        AddMsg( TFilter, TMsg, "PIECE_PART_NO"  , "PIECE_PART_NO"                       );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",            "\"",   "\""    );
        AddMsg( TFilter, TMsg, "MAT_SPEC"       , "MAT_SPEC"                            );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                            );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                             );
        AddMsg( TFilter, TMsg, "MULTIEQPINFO"   , "MULTIEQPINFO",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "USED_MATINFO"   , "USED_MATINFO",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "BINNO"          , "BINNO"                               );
        AddMsg( TFilter, TMsg, "REQTYPE"        , "REQTYPE"                             );
        AddMsg( TFilter, TMsg, "EDB_MAT_MASTER_CHK", "EDB_MAT_MASTER_CHK"               );
        AddMsg( TFilter, TMsg, "BOAT_FLAG"      , "BOAT_FLAG"                           );
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MERGE" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MERGE HDR=(LOTmgr,%s,MERGE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );
        AddMsg( TFilter, TMsg, "SUBLOTID"       , "SUBLOTID",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "MERGETIMES"     , "MERGETIMES"                      );
        AddMsg( TFilter, TMsg, "VALIDATION"     , "VALIDATION"                      );
        AddMsg( TFilter, TMsg, "FRAMEMERGEFLAG" , "FRAMEMERGEFLAG"                  );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "FRMCONV"        , "FRMCONV"                         );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MCPMERGE" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MCPMERGE HDR=(LOTmgr,%s,MERGE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SUBLOTID"       , "SUBLOTID" );
        AddMsg( TFilter, TMsg, "PKGTYPE"        , "PKGTYPE"  );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"      );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MCPTKIN" )  // MCPMERGE + TKIN
    {
        //MCPTKIN HDR=(,,) LOTID= OPERID= SUBLOTID= EQPID= [PKGTYPE=] [QTY=] [EQPTYPE=] [STEPSEQ=] [REQ_SYSTEM=] [EQPSTATUS=] 
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SUBLOTID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
//        if( CheckPara(TFilter, "EQPID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }  

        TMsg.Format((char *)"MCPTKIN HDR=(LOTmgr,%s,MCPTKIN) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SUBLOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SUBLOTID") );

        AddMsg( TFilter, TMsg, "PKGTYPE"        , "PKGTYPE"  );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"      );
        AddMsg( TFilter, TMsg, "EQPTYPE"        , "EQPTYPE"  );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"  );
        AddMsg( TFilter, TMsg, "EQPSTATUS"      , "EQPSTATUS");

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SCRAP" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SCRAPINFO"  , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SCRAP HDR=(LOTmgr,%s,SCRAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SCRAPINFO=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SCRAPINFO") );

        AddMsg( TFilter, TMsg, "TYPE"           , "TYPE"                            );
        AddMsg( TFilter, TMsg, "SUBSCRAPQTY"    , "SUBSCRAPQTY"                     );
        AddMsg( TFilter, TMsg, "STEPHANDLE"     , "STEPHANDLE"                      );
        AddMsg( TFilter, TMsg, "ARRAY_X"        , "ARRAY_X"                         );
        AddMsg( TFilter, TMsg, "ARRAY_Y"        , "ARRAY_Y"                         );
        AddMsg( TFilter, TMsg, "MAPINFO"        , "MAPINFO"                         );
        AddMsg( TFilter, TMsg, "FRAMEID"        , "FRAMEID"                         );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                        );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "SUBCMDS"        , "SUBCMDS"                         );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "PKGID"          , "PKGID"                           );
        AddMsg( TFilter, TMsg, "REPLY_SUBJECT"  , "REPLY_SUBJECT"                   );
        AddMsg( TFilter, TMsg, "DOTFLAG"        , "DOTFLAG"                         );
        AddMsg( TFilter, TMsg, "SCRAPWFINFO"    , "SCRAPWFINFO",    "(",    ")"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SPLIT_INCANCEL" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SPLITQTY"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SPLIT_INCANCEL HDR=(LOTmgr,%s,SPLIT_INCANCEL) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SPLITQTY=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SPLITQTY") );

        AddMsg( TFilter, TMsg, "SPLITLOTID"     , "SPLITLOTID"                      );
        AddMsg( TFilter, TMsg, "SPLITTIMES"     , "SPLITTIMES"                      );
        AddMsg( TFilter, TMsg, "SPLITTYPE"      , "SPLITTYPE"                       );
        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO",        "(",    ")"     );
        AddMsg( TFilter, TMsg, "SPLITGUBUN"     , "SPLITGUBUN"                      );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "WAFERQTY"       , "WAFERQTY"                        );
        AddMsg( TFilter, TMsg, "SPLITWFINFO"    , "SPLITWFINFO",    "(",    ")"     );
        AddMsg( TFilter, TMsg, "RETEST_YN"      , "RETEST_YN"                       );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SPLIT" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SPLIT HDR=(LOTmgr,%s,SPLIT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SPLITQTY"       , "SPLITQTY"                        );
        AddMsg( TFilter, TMsg, "TYPE"           , "TYPE"                            );
        AddMsg( TFilter, TMsg, "SPLITGUBUN"     , "SPLITGUBUN"                      );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "MERGEPROCID"    , "MERGEPROCID"                     );
        AddMsg( TFilter, TMsg, "MERGEINSTNO"    , "MERGEINSTNO"                     );
        AddMsg( TFilter, TMsg, "MERGERECIPEID"  , "MERGERECIPEID"                   );
        AddMsg( TFilter, TMsg, "SPLITLOTID"     , "SPLITLOTID"                      );
        AddMsg( TFilter, TMsg, "SBLFLAG"        , "SBLFLAG"                         );
        AddMsg( TFilter, TMsg, "SPLITTYPE"      , "SPLITTYPE"                       );
        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO",        "(",    ")"     );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "\"",   "\""    );
        AddMsg( TFilter, TMsg, "SPLITWFINFO"    , "SPLITWFINFO",    "(",    ")"     );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "RETEST_YN"      , "RETEST_YN"                       );
        AddMsg( TFilter, TMsg, "SPLITTIMES"     , "SPLITTIMES"                      );
        AddMsg( TFilter, TMsg, "WAFERQTY"       , "WAFERQTY"                        );
        AddMsg( TFilter, TMsg, "LAST_LOT_FIX_YN", "LAST_LOT_FIX_YN"                 );
        AddMsg( TFilter, TMsg, "PCBSERIAL"      , "PCBSERIAL"                       );
        AddMsg( TFilter, TMsg, "SPLIT_OPTION"   , "SPLIT_OPTION"                    );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SBLOUT" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SBLOUT HDR=(LOTmgr,%s,SBLOUT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "MCPINFO"        , "MCPINFO",        "(",    ")"     );
        AddMsg( TFilter, TMsg, "STEPSKIPINFO"   , "STEPSKIPINFO",   "(",    ")"     );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "GOODQTY"        , "GOODQTY"                         );
        AddMsg( TFilter, TMsg, "SCRAPINFO"      , "SCRAPINFO",      "(",    ")"     );
        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO",        "(",    ")"     );
        AddMsg( TFilter, TMsg, "SBLCODE"        , "SBLCODE"                         );
        AddMsg( TFilter, TMsg, "SBLYLD"         , "SBLYLD"                          );
        AddMsg( TFilter, TMsg, "SBL1"           , "SBL1"                            );
        AddMsg( TFilter, TMsg, "SBL2"           , "SBL2"                            );
        AddMsg( TFilter, TMsg, "SBL3"           , "SBL3"                            );
        AddMsg( TFilter, TMsg, "SBLCMD"         , "SBLCMD"                          );
        AddMsg( TFilter, TMsg, "CODE"           , "CODE"                            );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "MULTIFLAG"      , "MULTIFLAG"                       );
        AddMsg( TFilter, TMsg, "REASONCODE"     , "REASONCODE"                      );
        AddMsg( TFilter, TMsg, "HOLDFLAG"       , "HOLDFLAG"                        );
        AddMsg( TFilter, TMsg, "SBLFLAG"        , "SBLFLAG"                         );
        AddMsg( TFilter, TMsg, "SBLBIN"         , "SBLBIN"                          );
        AddMsg( TFilter, TMsg, "SBLLIMIT"       , "SBLLIMIT"                        );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MODATTR" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ATTR"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MODATTR HDR=(LOTmgr,%s,MODATTR) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ATTR=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"ATTR") );

        AddMsg( TFilter, TMsg, "ATTRNAME"       , "ATTRNAME"                        );
        AddMsg( TFilter, TMsg, "ATTRVALUE"      , "ATTRVALUE"                       );
        AddMsg( TFilter, TMsg, "BATCHID"        , "BATCHID"                         );
        AddMsg( TFilter, TMsg, "FLAG"           , "FLAG"                            );
        AddMsg( TFilter, TMsg, "STARTPOSITION"  , "STARTPOSITION"                   );
        AddMsg( TFilter, TMsg, "ENFORCE"        , "ENFORCE"                         );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "COMP_QTY_SYNC"  , "COMP_QTY_SYNC"                   );


        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PARTCHG" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "PARTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MODATTR HDR=(LOTmgr,%s,MODATTR) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s PARTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"PARTID") );

        AddMsg( TFilter, TMsg, "OLDPARTID"  , "OLDPARTID"                   );
        AddMsg( TFilter, TMsg, "STEPSEQ"    , "STEPSEQ"                     );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR"                        );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"                    );
        AddMsg( TFilter, TMsg, "TXNTIME"    , "TXNTIME"                     );
        AddMsg( TFilter, TMsg, "ACTION"     , "ACTION"                      );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PVIGATECHK" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PVIGATECHK HDR=(LOTmgr,%s,PVIGATECHK) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "REQTYPES", "REQTYPES");

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PVIGATEREGIST" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STEPSEQ"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "INSP_RESULT"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PVIGATEREGIST HDR=(LOTmgr,%s,PVIGATEREGIST) LINEID=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s STEPSEQ=%s INSP_RESULT=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"STEPSEQ"),
                TFilter.GetArg((char *)"INSP_RESULT") );

        AddMsg( TFilter, TMsg, "INSPEQPID"  , "INSPEQPID"                   );
        AddMsg( TFilter, TMsg, "SAMPLE_QTY" , "SAMPLE_QTY"                  );
        AddMsg( TFilter, TMsg, "INSPINFO"   , "INSPINFO",   "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"                    );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "RELEASE" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"RELEASE HDR=(LOTmgr,%s,RELEASE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "MULTIFLAG"  , "MULTIFLAG"                   );
        AddMsg( TFilter, TMsg, "CODE"       , "CODE"                        );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "REQTYPE"    , "REQTYPE"                     );
        AddMsg( TFilter, TMsg, "TXNTIME"    , "TXNTIME"                     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "REPOSITION" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STEPHANDLE"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"REPOSITION HDR=(LOTmgr,%s,REPOSITION) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s STEPHANDLE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"STEPHANDLE") );

        AddMsg( TFilter, TMsg, "REWORK"     , "REWORK"                      );
        AddMsg( TFilter, TMsg, "REASONCODE" , "REASONCODE"                  );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR"   ,    "(",    ")"     );
        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"                    );
        AddMsg( TFilter, TMsg, "TXNTIME"    , "TXNTIME"                     );
        AddMsg( TFilter, TMsg, "REJOIN_STEP"    , "REJOIN_STEP"                     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "STACKMERGE" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SUBLOTID"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"STACKMERGE HDR=(LOTmgr,%s,STACKMERGE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SUBLOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SUBLOTID") );

        AddMsg( TFilter, TMsg, "TOPARTID"   , "TOPARTID"                    );
        AddMsg( TFilter, TMsg, "CARRIERID"  , "CARRIERID"                   );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "REQTYPE"    , "REQTYPE"                     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "STORE" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "INVENTORYID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"STORE HDR=(LOTmgr,%s,STORE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s INVENTORYID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"INVENTORYID") );

        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",    "\"",   "\""    );
        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "REQTYPE"    , "REQTYPE"                     );
        AddMsg( TFilter, TMsg, "TXNTIME"    , "TXNTIME"                     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TKINCANCEL" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TKINCANCEL HDR=(LOTmgr,%s,TKINCANCEL) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "UNSCRAPINFO"    , "UNSCRAPINFO",    "(",    ")"     );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                        );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "PORTID"         , "PORTID"                          );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TKOUT" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TKOUT HDR=(LOTmgr,%s,TKOUT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SCRAPINFO"      , "SCRAPINFO"   ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "METALINFO"      , "METALINFO"   ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO"     ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "WAFERINFO"      , "WAFERINFO"   ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "DPKGQTY"        , "DPKGQTY"                         );
        AddMsg( TFilter, TMsg, "CHILDLOT"       , "CHILDLOT"                        );
        AddMsg( TFilter, TMsg, "BATCHLOTFLAG"   , "BATCHLOTFLAG"                    );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR"   ,        "(",    ")"     );
        AddMsg( TFilter, TMsg, "STEPSKIP"       , "STEPSKIP"                        );
        AddMsg( TFilter, TMsg, "HOLDCODE"       , "HOLDCODE"                        );
        AddMsg( TFilter, TMsg, "WORKORDERDEL"   , "WORKORDERDEL"                    );
        AddMsg( TFilter, TMsg, "NEXTSTEPSEQ"    , "NEXTSTEPSEQ"                     );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"                         );
        AddMsg( TFilter, TMsg, "PARTID"         , "PARTID"                          );
        AddMsg( TFilter, TMsg, "EQTYPE"         , "EQTYPE"                          );
        AddMsg( TFilter, TMsg, "GOODQTY"        , "GOODQTY"                         );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                             );
        AddMsg( TFilter, TMsg, "REWORK_STEP"    , "REWORK_STEP"                     );
        AddMsg( TFilter, TMsg, "REJOIN_STEP"    , "REJOIN_STEP"                     );
        AddMsg( TFilter, TMsg, "PCBINFO"        , "PCBINFO"                         );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "FRMCONV"        , "FRMCONV"                         );
        AddMsg( TFilter, TMsg, "BADCODE"        , "BADCODE"         ,   "(",    ")" );
        AddMsg( TFilter, TMsg, "BADQTY"         , "BADQTY"          ,   "(",    ")" );
        AddMsg( TFilter, TMsg, "SCRAPPRODSERIAL", "SCRAPPRODSERIAL" ,   "(",    ")" );
        AddMsg( TFilter, TMsg, "CHK_WF_QTY"     , "CHK_WF_QTY"                      );
        AddMsg( TFilter, TMsg, "BOAT_FLAG"      , "BOAT_FLAG"                       );
        AddMsg( TFilter, TMsg, "FIRST_PCB"      , "FIRST_PCB"                       );
        AddMsg( TFilter, TMsg, "KEEP_SETID"     , "KEEP_SETID"                      );
        AddMsg( TFilter, TMsg, "SBLYN"          , "SBLYN"                           );
        AddMsg( TFilter, TMsg, "BADCODE"        , "BADCODE"   ,     "(",    ")"     );
        AddMsg( TFilter, TMsg, "BADQTY"         , "BADQTY"   ,      "(",    ")"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYDISPLAY" )
    {
        //  CMD=TRAYDISPLAY PROCESS=%s EQPID=%s OPERID=%s PRODID=%s

        if( CheckPara(TFilter, "PRODID" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TRAYDISPLAY HDR=(LOTmgr,%s,TRAYDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                                "PRODID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"PRODID") );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYMAP" )
    {
        if( CheckPara(TFilter, "TRAYID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ARRAY_X"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ARRAY_Y"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "MAPINFO"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TRAYMAP HDR=(LOTmgr,%s,TRAYMAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "TRAYID=%s ARRAY_X=%s ARRAY_Y=%s MAPINFO=\"%s\" ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"TRAYID"),
                TFilter.GetArg((char *)"ARRAY_X"),
                TFilter.GetArg((char *)"ARRAY_Y"),
                TFilter.GetArg((char *)"MAPINFO") );

        AddMsg( TFilter, TMsg, "LOTID"      , "LOTID"                               );
        AddMsg( TFilter, TMsg, "PKGID"      , "PKGID"                               );
        AddMsg( TFilter, TMsg, "MAPUNIT"    , "MAPUNIT"                             );
        AddMsg( TFilter, TMsg, "MAPTYPE"    , "MAPTYPE"                             );
        AddMsg( TFilter, TMsg, "REQTYPE"    , "REQTYPE"                             );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYMAP_MODULE" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "MERGEINFO"  , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TRAYMAP HDR=(LOTmgr,%s,TRAYMAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s MERGEINFO=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"MERGEINFO") );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "UNSCRAP" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"UNSCRAP HDR=(LOTmgr,%s,UNSCRAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "UNSCRAPINFO"    , "UNSCRAPINFO",    "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                        );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "WAFERSORTINFO" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WAFERSORTINFO HDR=(LOTmgr,%s,WAFERSORTINFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SPLIT_COMPLT_YN"    , "SPLIT_COMPLT_YN"             );      
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "WORKORDERDEL" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WORKORDERDEL HDR=(LOTmgr,%s,WORKORDERDEL) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "ACTIVITY"   , "ACTIVITY"  );
        AddMsg( TFilter, TMsg, "CARRIERID"  , "CARRIERID" );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CARRIERID_READ" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CARRIERID_READ HDR=(LOTmgr,%s,CARRIERID_READ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "PORTID",                "PORTID"                    );
        AddMsg( TFilter, TMsg, "CARRIERID",             "CARRIERID"                 );
        AddMsg( TFilter, TMsg, "CARRIERID_REMOVE_FLAG", "CARRIERID_REMOVE_FLAG"     );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "CREATE_GROUP" )
    {
        TMsg.Format((char *)"CREATE_GROUP HDR=(LOTmgr,%s,CREATE_GROUP) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID") );

        AddMsg( TFilter, TMsg, "LOTID"      , "LOTID"     );
        AddMsg( TFilter, TMsg, "GROUPID"    , "GROUPID"   );
        AddMsg( TFilter, TMsg, "GROUPTYPE"  , "GROUPTYPE" );
        AddMsg( TFilter, TMsg, "MULTILOTS"  , "MULTILOTS" );
        AddMsg( TFilter, TMsg, "PORTID"     , "PORTID"    ); 
        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EMPTY_CARRIER_MOVE_REQ" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EMPTY_CARRIER_MOVE_REQ HDR=(LOTmgr,%s,EMPTY_CARRIER_MOVE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        AddMsg( TFilter, TMsg, "LOTID"      , "LOTID"       );
        AddMsg( TFilter, TMsg, "STEP"       , "STEP"        );
        AddMsg( TFilter, TMsg, "LINE"       , "LINE"        );
        AddMsg( TFilter, TMsg, "SUBLINE"    , "SUBLINE"     );
        AddMsg( TFilter, TMsg, "PARTNUMBER" , "PARTNUMBER"  );
        AddMsg( TFilter, TMsg, "MCPSEQ"     , "MCPSEQ"      );
        AddMsg( TFilter, TMsg, "TO_PART"    , "TO_PART"     );
        AddMsg( TFilter, TMsg, "TRAYSPEC"   , "TRAYSPEC"    );
        AddMsg( TFilter, TMsg, "CARRIERID"  , "CARRIERID"   );
        AddMsg( TFilter, TMsg, "RPROPERTY"  , "RPROPERTY"   );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EQP_STATUS_REPORT" )
    {
        if( CheckPara(TFilter, "EQPSTAT"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STATCODE"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EQP_STATUS_REPORT HDR=(LOTmgr,%s,EQP_STATUS_REPORT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "EQPSTAT=%s STATCODE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPSTAT"),
                TFilter.GetArg((char *)"STATCODE") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "FULL_CARRIER_MOVE_REQ" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"FULL_CARRIER_MOVE_REQ HDR=(LOTmgr,%s,FULL_CARRIER_MOVE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        AddMsg( TFilter, TMsg, "LOTID", "LOTID" );  
        AddMsg( TFilter, TMsg, "CARRIERID"  , "CARRIERID" );
        AddMsg( TFilter, TMsg, "LINE",        "LINE"      );
        AddMsg( TFilter, TMsg, "SUBLINE"    , "SUBLINE"   );
        AddMsg( TFilter, TMsg, "CARRIER_TYPE"        , "CARRIER_TYPE"   );
        AddMsg( TFilter, TMsg, "SUB_CARRIER_TYPE"    , "SUB_CARRIER_TYPE"   );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "LOC_INFO_CHANGE_REQ" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOC_INFO_CHANGE_REQ HDR=(LOTmgr,%s,LOC_INFO_CHANGE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        AddMsg( TFilter, TMsg, "AUTODISPATCHFLAG"   , "AUTODISPATCHFLAG" );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "PORT_INFO_CHANGE_REQ" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PORT_INFO_CHANGE_REQ HDR=(LOTmgr,%s,PORT_INFO_CHANGE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        AddMsg( TFilter, TMsg, "TOOLSTATUS"         , "TOOLSTATUS"          );
        AddMsg( TFilter, TMsg, "WAITLOTCNT"         , "WAITLOTCNT"          );
        AddMsg( TFilter, TMsg, "PORTLOTID"          , "PORTLOTID"           );
        AddMsg( TFilter, TMsg, "PORTCARRIERID"      , "PORTCARRIERID"       );
        AddMsg( TFilter, TMsg, "AUTODISPATCHFLAG"   , "AUTODISPATCHFLAG"    );
        AddMsg( TFilter, TMsg, "VEH_CALL_TYPE"      , "VEH_CALL_TYPE"       );
        AddMsg( TFilter, TMsg, "REQ_MATL_TYPE"      , "REQ_MATL_TYPE"       );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "PORTDISPLAY" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PORTDISPLAY HDR=(LOTmgr,%s,PORTDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "READY_TO_WORK" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"READY_TO_WORK HDR=(LOTmgr,%s,READY_TO_WORK) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "EQPSTATUS"  , "EQPSTATUS"                   );
        AddMsg( TFilter, TMsg, "CARRIERID"  , "CARRIERID"                   );
        AddMsg( TFilter, TMsg, "PORTID"     , "PORTID"                      );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "REFLOW_RECIPE_ID"  , "REFLOW_RECIPE_ID"     );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "WORK_COMP" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WORK_COMP HDR=(LOTmgr,%s,WORK_COMP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "GOODQTY"    , "GOODQTY"                         );
        AddMsg( TFilter, TMsg, "SCRAPINFO"  , "SCRAPINFO",      "(",    ")"     );
        AddMsg( TFilter, TMsg, "PORTID"     , "PORTID"                          );
        AddMsg( TFilter, TMsg, "CARRIERID"  , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "LASTFLAG"   , "LASTFLAG"                        );
        AddMsg( TFilter, TMsg, "REQ_RULETYPE","REQ_RULETYPE"                    );
        AddMsg( TFilter, TMsg, "WM_FLOWTYPE", "WM_FLOWTYPE"                     );
        AddMsg( TFilter, TMsg, "BININFO"    , "BININFO"                         );
        AddMsg( TFilter, TMsg, "SCRAPINFO"  , "SCRAPINFO"                       );
        AddMsg( TFilter, TMsg, "SPLITWFINFO", "SPLITWFINFO"                     );
        AddMsg( TFilter, TMsg, "SPLITQTY"   , "SPLITQTY"                        );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR"                            );
        AddMsg( TFilter, TMsg, "REQTYPES"   , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "HOLDCODE"   , "HOLDCODE"                        );
        AddMsg( TFilter, TMsg, "CHK_WF_QTY" , "CHK_WF_QTY"                      );
        AddMsg( TFilter, TMsg, "SUBLINE"    , "SUBLINE"                         );
        AddMsg( TFilter, TMsg, "TESTINFO"   , "TESTINFO"                        );
        AddMsg( TFilter, TMsg, "AUTO_WAFER_YN"   , "AUTO_WAFER_YN"              );
        AddMsg( TFilter, TMsg, "CHK_WAFER_LIST"  , "CHK_WAFER_LIST", "(",    ")");
        AddMsg( TFilter, TMsg, "CHK_RING_LIST"   , "CHK_RING_LIST",  "(",    ")");
        AddMsg( TFilter, TMsg, "SPLITTYPE"  , "SPLITTYPE"                       );
        AddMsg( TFilter, TMsg, "SPACERQTY"  , "SPACERQTY"                       );
        AddMsg( TFilter, TMsg, "CARRIERID_REMOVE_FLAG", "CARRIERID_REMOVE_FLAG" );
        AddMsg( TFilter, TMsg, "MK_OUTQTY"  , "MK_OUTQTY"                       );
        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "WORK_START_REQ_REP" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STATUS"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WORK_START_REQ_REP HDR=(LOTmgr,%s,WORK_START_REQ_REP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s STATUS=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"STATUS") );

        AddMsg( TFilter, TMsg, "CARRIERID", "CARRIERID" );
        AddMsg( TFilter, TMsg, "PORTID"   , "PORTID"    );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "GET_SIMAXDATA" )
    {
        if( CheckPara(TFilter, "SQL_ID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"GET_SIMAXDATA HDR=(LOTmgr,%s,GET_SIMAXDATA) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "SQL_ID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SQL_ID") );

        AddMsg( TFilter, TMsg, "VALUE"      , "VALUE"                       ); 
        AddMsg( TFilter, TMsg, "SQL_DESC"   , "SQL_DESC",   "\"",   "\""    ); 

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "EC_MOVE_ORDER" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CARRIERID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EC_MOVE_ORDER HDR=(LOTmgr,%s,EC_MOVE_ORDER) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CARRIERID=%s LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CARRIERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "FROMEQPID"      , "FROMEQPID"       );
        AddMsg( TFilter, TMsg, "FROMPORTID"     , "FROMPORTID"      );
        AddMsg( TFilter, TMsg, "PORTID"         , "PORTID"          );
        AddMsg( TFilter, TMsg, "TODEVICETYPE"   , "TODEVICETYPE"    );
        AddMsg( TFilter, TMsg, "COMPONENT_QTY"  , "COMPONENT_QTY"   );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EQP_START_REQ" )
    {
        TMsg.Format((char *)"EQP_START_REQ HDR=(LOTmgr,%s,EQP_START_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID") );

        AddMsg( TFilter, TMsg, "DIAGSTART"  , "DIAGSTART" ); 
        AddMsg( TFilter, TMsg, "EQPLOTINFO" , "EQPLOTINFO", "\"", "\"" );  
        AddMsg( TFilter, TMsg, "ITEMNAME"   , "ITEMNAME",   "\"", "\"" );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EQUIP_DEST_REQ" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EQUIP_DEST_REQ HDR=(LOTmgr,%s,EQUIP_DEST_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "CARRIERID", "CARRIERID" );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "ETRAY_CARRIER_MOVE_REQ" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"ETRAY_CARRIER_MOVE_REQ HDR=(LOTmgr,%s,ETRAY_CARRIER_MOVE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        AddMsg( TFilter, TMsg, "LINE",    "LINE"    );
        AddMsg( TFilter, TMsg, "SUBLINE", "SUBLINE" );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "IMS_GETTRAYINFO" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CARRIERID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_GETTRAYINFO HDR=(LOTmgr,%s,IMS_GETTRAYINFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s CARRIERID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"CARRIERID") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "LAST_WAFER_INFO" )
    {
        if( CheckPara(TFilter, "PORTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LAST_WAFER_INFO HDR=(LOTmgr,%s,LAST_WAFER_INFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID") );

        AddMsg( TFilter, TMsg, "EQPTYPE"    , "EQPTYPE"     );
        AddMsg( TFilter, TMsg, "WAFERCNT"   , "WAFERCNT"    );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "PORT_CHANGE_REQ" )
    {
        if( CheckPara(TFilter, "PORTSTATUS"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PORT_CHANGE_REQ HDR=(LOTmgr,%s,PORT_CHANGE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTSTATUS=<%s> ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTSTATUS") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "SBL_RESULT_INFO" )
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SBLYN"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SBL_RESULT_INFO HDR=(LOTmgr,%s,SBL_RESULT_INFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SBLYN=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SBLYN") );

        AddMsg( TFilter, TMsg, "SETID"      , "SETID"                           );
        AddMsg( TFilter, TMsg, "REASONCODE" , "REASONCODE"                      );
        AddMsg( TFilter, TMsg, "SBL1"       , "SBL1"                            );
        AddMsg( TFilter, TMsg, "SBL2"       , "SBL2"                            );
        AddMsg( TFilter, TMsg, "SBL3"       , "SBL3"                            );
        AddMsg( TFilter, TMsg, "HOLDFLAG"   , "HOLDFLAG"                        );
        AddMsg( TFilter, TMsg, "SBLCMD"     , "SBLCMD"                          );
        AddMsg( TFilter, TMsg, "CODE"       , "CODE"                            );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR",           "<",    ">"     );
        AddMsg( TFilter, TMsg, "COMMENT"    , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "DELETE_SETID", "DELETE_SETID"                   );
        AddMsg( TFilter, TMsg, "KEEP_SETID"  , "KEEP_SETID"                     );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "SET_LOCATION_REQ" )
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SET_LOCATION_REQ HDR=(LOTmgr,%s,SET_LOCATION_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "STACKER_CHANGE_REQ" )
    {
        if( CheckPara(TFilter, "PORTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "PORTSTATUS" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"STACKER_CHANGE_REQ HDR=(LOTmgr,%s,STACKER_CHANGE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "PORTID=%s PORTSTATUS=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID"),
                TFilter.GetArg((char *)"PORTSTATUS") );

        AddMsg( TFilter, TMsg, "STACKER_TYPE", "STACKER_TYPE" );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "WAFER_WORK_COMPLETE" )
    {
        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WAFER_WORK_COMPLETE HDR=(LOTmgr,%s,WAFER_WORK_COMPLETE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "WAITLOT_CHECK_REQ" )
    {
        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WAITLOT_CHECK_REQ HDR=(LOTmgr,%s,WAITLOT_CHECK_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "AGVCALLFLAG", "AGVCALLFLAG" );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EQPDISPLAY" )
    {
        //  EQPDISPLAY HDR=(LOTmgr,L1.OYTESTJT,AA) EQPID=4WB003 OPERID=AUTO_KJT
        //  EQPDISPLAY_REP HDR=(LOTmgr,L1.OYTESTJT,AA) STATUS=PASS EQPID=4WB003 MODE=ENABLE CODE= EQPSTATUS=ENABLE

        if( CheckPara(TFilter, "EQPID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EQPDISPLAY HDR=(LOTmgr,%s,EQPDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "EQPDISPLAY_SIMAX" )
    {
        //  EQPDISPLAY HDR=(LOTmgr,L1.OYTESTJT,AA) EQPID=4WB003 OPERID=AUTO_KJT
        //  EQPDISPLAY_REP HDR=(LOTmgr,L1.OYTESTJT,AA) STATUS=PASS EQPID=4WB003 MODE=ENABLE CODE= EQPSTATUS=ENABLE

        if( CheckPara(TFilter, "EQPID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EQPDISPLAY HDR=(LOTmgr,%s,EQPDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }    
    else if( TCmd == "PACKING" )
    {
        //  PACKING HDR=(TRACKING,L1.OYTESTCH,AA) LOTID=TESTCH2 OPERID=88648864 LABELTYPE=SBOX WORKTYPE=NEW WORKLINEID=1 SUBLINEID=1

        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING HDR=(LOTmgr,%s,PACKING) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "LABELTYPE"  , "LABELTYPE"  );
        AddMsg( TFilter, TMsg, "WORKTYPE"   , "WORKTYPE"   );
        AddMsg( TFilter, TMsg, "WORKLINEID" , "WORKLINEID" );
        AddMsg( TFilter, TMsg, "SUBLINEID"  , "SUBLINEID"  );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "PACKINGLABEL" )
    {
        //  PACKINGLABEL HDR=(CCHTRACKING,L1.OYTESTCH,AA) LOTID=SIMAXJT999 OPERID=AUTO BOXTYPE=SBOX SEQ=ALL

        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKINGLABEL HDR=(LOTmgr,%s,PACKINGLABEL) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "BOXTYPE" , "BOXTYPE" );
        AddMsg( TFilter, TMsg, "SEQ"     , "SEQ"     );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "GET_VIRTUALID_REQ" )
    {
        TMsg.Format((char *)"GET_VIRTUALID_REQ HDR=(LOTmgr,%s,GET_VIRTUALID_REQ) LINEID=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID") );

        AddMsg( TFilter, TMsg, "TYPE"           , "TYPE"        );
        AddMsg( TFilter, TMsg, "KEYVALUE"       , "KEYVALUE"    );
        AddMsg( TFilter, TMsg, "BOARDTYPE"      , "BOARDTYPE"   );
        AddMsg( TFilter, TMsg, "SERIAL"         , "SERIAL"      );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "LABEL_PRINT" )
    {
        //  CMD=LABEL_PRINT CLIENTIP=12.98.232.41 CLIENTPORT=6020 SENDAPP=YOBSSLPN TOAPP=YOBXSLPN LOTID=GKHA509VL USERID=02042311 PRINTCHECK=Y

        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CLIENTIP=%s CLIENTPORT=%s SENDAPP=%s TOAPP=%s DATA={USERID=%s,LOTID=%s,PRINTCHECK=%s}",
            TFilter.GetArg((char *)"CLIENTIP"), TFilter.GetArg((char *)"CLIENTPORT"), TFilter.GetArg((char *)"SENDAPP"),
            TFilter.GetArg((char *)"TOAPP"), TFilter.GetArg((char *)"USERID"), TFilter.GetArg((char *)"LOTID"), TFilter.GetArg((char *)"PRINTCHECK")); 

        TMesTarget = "SIMAX_OI";
    }
    else if( TCmd == "WAFERDISPLAY" )
    {
        //  CMD=WAFERDISPLAY PROCESS=%s EQPID=%s OPERID=%s MES_TARGET=%s LOTID=%s TYPE=WAFER

        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WAFERDISPLAY HDR=(LOTmgr,%s,WAFERDISPLAY) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "TYPE"       , "TYPE"                        );
        AddMsg( TFilter, TMsg, "SLOTINFO"   , "SLOTINFO"                    );
        AddMsg( TFilter, TMsg, "ATTR"       , "ATTR",       "(",    ")"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MCPDISPLAY" )
    {
        //  CMD=MCPDISPLAY PROCESS=%s EQPID=%s OPERID=%s MES_TARGET=%s LOTID=%s

        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MCPDISPLAY HDR=(LOTmgr,%s,MCPDISPLAY) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "SIMAX";
    }else if( TCmd == "PACKING_SBOX" )
    {
        //  PACKING_SBOX HDR=(LOTmgr,L1.OYTESTCCH,OSSUIM) LOTID=SIMAXXTS11 OPERID=OPER01 REQ_SYSTEM=TC WORKLINEID=4 SUBLINEID=4
        //  PACKING_SBOX_REP HDR=(LOTmgr,L1.OYTESTCCH,OSSUIM) STATUS=PASS LOTID=SIMAXXTS11 PART-NO=K9K8G08U0B-PCB0TJP-8AX8UH LARGE_BOX=N LBOX_VID=- BOX_CNT=2 LABEL_CNT=2 
        //    LOT_QTY=1920 LOT_STEP=- TRAY_THICK=6.35 TR_ARRAY_X=8 TR_ARRAY_Y=12 
        //    BOX01_LABEL_TYPE_1="SEC" 
        //    BOX01_LABEL_FORMAT_1="SIMAXXTS1101000153101" 
        //    BOX01_LABEL_TYPE_2="WIRELESS" 
        //    BOX01_LABEL_FORMAT_2="SIMAXXTS1101000153101" 
        //    BOX02_LABEL_TYPE_1="SEC" 
        //    BOX02_LABEL_FORMAT_1="SIMAXXTS1100920153102" 
        //    BOX02_LABEL_TYPE_2="WIRELESS" 
        //    BOX02_LABEL_FORMAT_2="SIMAXXTS1100920153102" 
        //    SBOXBARSTR_1="^XA^LH000,000^FS^FO133,370^AD,30,20^FDOPER01^FS^FO628,298^AD,30,20^FD01^FS^FO325,299^AD,30,20^FD1000^FS^FO122,175^AD,30,20^FDK9K8G08U0B-PCB0TJP^FS^FO273,300^AD,30,20^FDQ'TY^FS^FO600,42^AD,10,10^FD08:21^FS^FO600,30^AD,10,10^FD15/08/21^FS^FO34,217^BY2,2,72^B3,,,N^FDSIMAXXTS1100000153101^FS^FO39,106^BY2,2,48^B3,,,N^FDK9K8G08U0B-PCB0TJP  ^FS^FO37,406^AD,25,35^FDASSEMBLED IN KOREA FROM DIE OF KOREA^FS^FO35,297^AD,30,20^FDLOT:^FS~DGROHSMARK,3000,46,000001FFFF00000000000000000000000000000000,00003FFFFFF8000000000000000000000000000000,00007FFFFFFC000000000000000000000000000000,0001FFFFFFFF000000000000000000000000000000,0003FFFFFFFF800000000000000000000000000000,001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC00,003FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE00,007FFFFFFFFFFC0000000000000000000000000E00,00FFFFFFFFFFFE0000000000000000000000000300,01FFFFFFFFFFFF0000000000000000000000000100,03FFFFFFFFFFFF8000000000000000000000000180,03FFFFFFFFFFFFC000000000000000000000000180,07FFFFFFFFFFFFC000000000000000000000000180,07FF3FFFFFFFFFC000000000000000000000000180,00073FFFFFFFFF2000000000000000000000000300,13C33FFFFFFFFF3000000000000000000000000300,13C33FFFFFFFFF3000000000000000000000000200,33E33E3F8FFC7C000FE0007061E000000000000600,73E33C0F00F008000FF8007067FC00000000000600,73E33FC738F38F3C0FFC0070673C00000000000E00,73C73FE73EE7CF3C0E1C7870671C00000000000E00,70073E073EE7CF3C0E1CFC70670000000000000E00,700F38073EE7CF3C0E1CFE7FE7C000000000000E00,F3FF31E73EE00F3E0FF9C77FE0FE00000000001C00,F3FF31E73EE7FF3E0E39C770603E00000000001C00,F3FF31E73EE7FF3E0E1DC770661E00000000001800,F3FF31E73EE3FF3E0E1DC770661E00000000001800,F3FF38073EF81F030E1CFE7067FC00000000003000,F3FF3C073EF81FC30E1CFC7063F800000000003000,FFFFFFFFFFFFFFFF0000380000E000000000003000,FFFFFFFFFFFFFFFF00000000000000000000003000,FFFFFDFFFFFFFFFF00000000000000000000007000,FFC019FFFFFFFFFF00000000000000000000006000,FFC009FFFFFFFFFF00000000000000000000006000,FFC7FFFFFFFF7FFF00000000000000000000006000,FFC7FFFFFFFF7FFF00000000000000000000004000,FFC7FDC0700C03FE07F8000000003300000700C000,FFC7FDC0300C03FE07FC000000003300000700C000,FFC7FDCF31C77FFE073C000000003000000701C000,FFC01DCF11E77FFE0E1C000000003000000701C000,7FC7FDCFF0FF7FFC1E00FF7FFCFF3339DFEFC38000,7FC7FDCFF81F7FFC1E00C779CCE33331DC77038000,7FC7FDCFFC0F7FFC1C00C771CCC1B301D877038000,7FC7FDCFFFE77FFC1E1DC771CCC1B31FD877038000,7FC7FDCFF3E7BFFC0E1CC771CCC1B331D877030000,3FC7FDCFF3E79FF8071CC771CCE33331D877030000,1FC7FDCFF00783F007F8FE71CCFE333FD877C20000,1FC7FDCFF80FC3F001E07C71CCDE331FD873C20000,0FFFFFFFFFFFFFE000C0000000C000000000060000,07FFFFFFFFFFFFC00000000000C000000000040000,07FFFFFFFFFFFFC00000000000C0000000000C0000,03FFFFFFFFFFFFC0000000000000000000000C0000,03FFFFFFFFFFFF80000000000000000000001C0000,01FFFFFFFFFFFF0000000000000000000000380000,00FFFFFFFFFFFE0000000000000000000000700000,007FFFFFFFFFFC0000000000000000000001E00000,003FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC00000,000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC000000,0003FFFFFFFF800000000000000000000000000000,0001FFFFFFFF000000000000000000000000000000,00007FFFFFFC000000000000000000000000000000,00003FFFFFF8000000000000000000000000000000,000001FFFF00000000000000000000000000000000,0000003FF000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000^FO729,222^XGROHSMARK,1,1^FS^FO38,175^AD,30,20^FDDVC :^FS^PQ1^XZ" 
        //    SBOXBARSTR_2="^XA^LH000,000^FS^FO133,370^AD,30,20^FDOPER01^FS^FO628,298^AD,30,20^FD02^FS^FO325,299^AD,30,20^FD920^FS^FO122,175^AD,30,20^FDK9K8G08U0B-PCB0TJP^FS^FO273,300^AD,30,20^FDQ'TY^FS^FO600,42^AD,10,10^FD08:21^FS^FO600,30^AD,10,10^FD15/08/21^FS^FO34,217^BY2,2,72^B3,,,N^FDSIMAXXTS1100000153102^FS^FO39,106^BY2,2,48^B3,,,N^FDK9K8G08U0B-PCB0TJP  ^FS^FO37,406^AD,25,35^FDASSEMBLED IN KOREA FROM DIE OF KOREA^FS^FO35,297^AD,30,20^FDLOT:^FS~DGROHSMARK,3000,46,000001FFFF00000000000000000000000000000000,00003FFFFFF8000000000000000000000000000000,00007FFFFFFC000000000000000000000000000000,0001FFFFFFFF000000000000000000000000000000,0003FFFFFFFF800000000000000000000000000000,001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC00,003FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE00,007FFFFFFFFFFC0000000000000000000000000E00,00FFFFFFFFFFFE0000000000000000000000000300,01FFFFFFFFFFFF0000000000000000000000000100,03FFFFFFFFFFFF8000000000000000000000000180,03FFFFFFFFFFFFC000000000000000000000000180,07FFFFFFFFFFFFC000000000000000000000000180,07FF3FFFFFFFFFC000000000000000000000000180,00073FFFFFFFFF2000000000000000000000000300,13C33FFFFFFFFF3000000000000000000000000300,13C33FFFFFFFFF3000000000000000000000000200,33E33E3F8FFC7C000FE0007061E000000000000600,73E33C0F00F008000FF8007067FC00000000000600,73E33FC738F38F3C0FFC0070673C00000000000E00,73C73FE73EE7CF3C0E1C7870671C00000000000E00,70073E073EE7CF3C0E1CFC70670000000000000E00,700F38073EE7CF3C0E1CFE7FE7C000000000000E00,F3FF31E73EE00F3E0FF9C77FE0FE00000000001C00,F3FF31E73EE7FF3E0E39C770603E00000000001C00,F3FF31E73EE7FF3E0E1DC770661E00000000001800,F3FF31E73EE3FF3E0E1DC770661E00000000001800,F3FF38073EF81F030E1CFE7067FC00000000003000,F3FF3C073EF81FC30E1CFC7063F800000000003000,FFFFFFFFFFFFFFFF0000380000E000000000003000,FFFFFFFFFFFFFFFF00000000000000000000003000,FFFFFDFFFFFFFFFF00000000000000000000007000,FFC019FFFFFFFFFF00000000000000000000006000,FFC009FFFFFFFFFF00000000000000000000006000,FFC7FFFFFFFF7FFF00000000000000000000006000,FFC7FFFFFFFF7FFF00000000000000000000004000,FFC7FDC0700C03FE07F8000000003300000700C000,FFC7FDC0300C03FE07FC000000003300000700C000,FFC7FDCF31C77FFE073C000000003000000701C000,FFC01DCF11E77FFE0E1C000000003000000701C000,7FC7FDCFF0FF7FFC1E00FF7FFCFF3339DFEFC38000,7FC7FDCFF81F7FFC1E00C779CCE33331DC77038000,7FC7FDCFFC0F7FFC1C00C771CCC1B301D877038000,7FC7FDCFFFE77FFC1E1DC771CCC1B31FD877038000,7FC7FDCFF3E7BFFC0E1CC771CCC1B331D877030000,3FC7FDCFF3E79FF8071CC771CCE33331D877030000,1FC7FDCFF00783F007F8FE71CCFE333FD877C20000,1FC7FDCFF80FC3F001E07C71CCDE331FD873C20000,0FFFFFFFFFFFFFE000C0000000C000000000060000,07FFFFFFFFFFFFC00000000000C000000000040000,07FFFFFFFFFFFFC00000000000C0000000000C0000,03FFFFFFFFFFFFC0000000000000000000000C0000,03FFFFFFFFFFFF80000000000000000000001C0000,01FFFFFFFFFFFF0000000000000000000000380000,00FFFFFFFFFFFE0000000000000000000000700000,007FFFFFFFFFFC0000000000000000000001E00000,003FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC00000,000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC000000,0003FFFFFFFF800000000000000000000000000000,0001FFFFFFFF000000000000000000000000000000,00007FFFFFFC000000000000000000000000000000,00003FFFFFF8000000000000000000000000000000,000001FFFF00000000000000000000000000000000,0000003FF000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000,000000000000000000000000000000000000000000^FO729,222^XGROHSMARK,1,1^FS^FO38,175^AD,30,20^FDDVC :^FS^PQ1^XZ" 
        //    SPECIALBARSTR_1="^XA^LH000,000^FS^FO273,300^AD,30,20^FDQ'TY^FS^FO35,297^AD,30,20^FDLOT:^FS^PQ1^XZ" 

        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING_SBOX HDR=(LOTmgr,%s,PACKING_SBOX) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                                "LOTID=%s WORKLINEID=%s SUBLINEID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"WORKLINEID"),
                    TFilter.GetArg((char *)"SUBLINEID") );
        AddMsg( TFilter, TMsg, "ZPLSTRFLAG"           , "ZPLSTRFLAG"    );
            AddMsg( TFilter, TMsg, "PACKGATE"             , "PACKGATE"      );

        TMesTarget = "SIMAX";
    }else if( TCmd == "PACKING_LABEL_SBOX" )     
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING_LABEL_SBOX HDR=(LOTmgr,%s,PACKING_LABEL_SBOX) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                                "LOTID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "BOXSEQ"             , "BOXSEQ"      );
        AddMsg( TFilter, TMsg, "LOTCLOSE"           , "LOTCLOSE"    );

        TMesTarget = "SIMAX";
    }else if( TCmd == "PACKING_DISPLAY" )     
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING_DISPLAY HDR=(LOTmgr,%s,PACKING_DISPLAY) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                             "LOTID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "BOXTYPE"             , "BOXTYPE"      );

        TMesTarget = "SIMAX";
    }else if( TCmd == "SLIPINFO" )
    {
        //SLIPINFO HDR=(TRACKING,LH.PACKING18771,SLIPINFO) LBOXID=OE81170036 LOT_ID=JCJ0679RAD OPERID=AUTO TYPE=LBOX REQ_SYSTEM=TC]
        //SLIPINFO_REP HDR=(TRACKING,LH.PACKING18771,SLIPINFO) STATUS=PASS PACKINGTYPE=0 LOTID=OE81170036 QTY=4480 SHIP_SITE=- HOLDCODE=- MULTIHOLDCODE=- REQ_SYSTEM=TC]
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SLIPINFO HDR=(LOTmgr,%s,SLIPINFO) EQPID=%s LBOXID=%s LOT_ID=%s TYPE=%s OPERID=AUTO REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LBOXID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"TYPE"));
        TMesTarget = "SIMAX";
    }else if( TCmd == "PACKING_LBOX" )
    {
        //SLIPINFO HDR=(TRACKING,LH.PACKING18771,SLIPINFO) LBOXID=OE81170036 LOT_ID=JCJ0679RAD OPERID=AUTO TYPE=LBOX REQ_SYSTEM=TC]
        //SLIPINFO_REP HDR=(TRACKING,LH.PACKING18771,SLIPINFO) STATUS=PASS PACKINGTYPE=0 LOTID=OE81170036 QTY=4480 SHIP_SITE=- HOLDCODE=- MULTIHOLDCODE=- REQ_SYSTEM=TC]
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING_LBOX HDR=(LOTmgr,%s,PACKING_LBOX) EQPID=%s LOTID=%s WORKLINEID=%s SUBLINEID=%s SBOXLIST=(%s) OPERID=AUTO REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"WORKLINEID"),
                    TFilter.GetArg((char *)"SUBLINEID"),
                    TFilter.GetArg((char *)"SBOXLIST"));

            AddMsg( TFilter, TMsg, "PACKGATE"             , "PACKGATE"      );

        TMesTarget = "SIMAX";
    }else if( TCmd == "PACKING_SLIP" )
    {
        //SLIPINFO HDR=(TRACKING,LH.PACKING18771,SLIPINFO) LBOXID=OE81170036 LOT_ID=JCJ0679RAD OPERID=AUTO TYPE=LBOX REQ_SYSTEM=TC]
        //SLIPINFO_REP HDR=(TRACKING,LH.PACKING18771,SLIPINFO) STATUS=PASS PACKINGTYPE=0 LOTID=OE81170036 QTY=4480 SHIP_SITE=- HOLDCODE=- MULTIHOLDCODE=- REQ_SYSTEM=TC]
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING_SLIP HDR=(LOTmgr,%s,PACKING_SLIP) EQPID=%s LOTID=%s WORKLINEID=%s SUBLINEID=%s LBOXLIST=%s MODE=%s OPERID=AUTO REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"WORKLINEID"),
                    TFilter.GetArg((char *)"SUBLINEID"),
                    TFilter.GetArg((char *)"LBOXLIST"),
                    TFilter.GetArg((char *)"MODE"));
        TMesTarget = "SIMAX";
    }else if( TCmd == "PACKING_SLIP_SEND" )
    {
        //PACKING_SLIP HDR=(IMSmgr,LH.PACKING17749,PACKING_SLIP) LOTID=JCJ0679RAD OPERID=AUTO REQ_SYSTEM=TC WORKLINEID=1 SUBLINEID=2 LBOXLIST=OE81170036 MODE= REQ_SYSTEM=TC
        //PACKING_SLIP_REP HDR=(IMSmgr,LH.PACKING17749,PACKING_SLIP) STATUS=PASS SLIPID=OE8117G036 SEMI_PROD=N REQ_SYSTEM=TC
        //PACKING_SLIP_SEND HDR=(IMSmgr,LH.PACKING17749,PACKING_SLIP) SLIPID=OE8117G036 OPERID=AUTO REQ_SYSTEM=TC
        //PACKING_SLIP_SEND_REP HDR=(IMSmgr,LH.PACKING17749,PACKING_SLIP) STATUS=PASS SLIPID=OE8117G036 SEMI_PROD=N REQ_SYSTEM=TC
        if( CheckPara(TFilter, "SLIPID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PACKING_SLIP_SEND HDR=(LOTmgr,%s,PACKING_SLIP_SEND) EQPID=%s SLIPID=%s OPERID=AUTO REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"SLIPID"));
        TMesTarget = "SIMAX";
    }else if( TCmd == "SLIPCHECK" )
    {
        //SLIPCHECK HDR=(IMSmgr,LH.PACKING18504,PACKING_SLIP_CHECK) LBOXID=OE81170036 OPERID=AUTO REQ_SYSTEM=TC]
        //SLIPCHECK_REP HDR=(IMSmgr,LH.PACKING18504,PACKING_SLIP_CHECK) STATUS=PASS LBOXID=OE81170036 SLIPID=OE8117G036 PRODID=K4EBE304EC-EGCF000-JMB7WZ CURRTOOLID= CURRPORTID= REQ_SYSTEM=TC]
        if( CheckPara(TFilter, "LBOXID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SLIPCHECK HDR=(LOTmgr,%s,SLIPCHECK) EQPID=%s LBOXID=%s OPERID=AUTO REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LBOXID"));
        TMesTarget = "SIMAX";
    }else if( TCmd == "TNR_FAIL_LABEL" )
    {
        //  TNR_FAIL_LABEL HDR=(LOTmgr,L1.OYTESTYSH3,OSSUIM) LOTID=CJHJH035 OPERID=AUTO
        //  TNR_FAIL_LABEL_REP HDR=(LOTmgr,L1.OYTESTYSH3,OSSUIM) STATUS=PASS LOTID=CJHJH035 LABELTEXT="^XA^LH000,000^POI^FS^FO200,105^AD^FDS3C89D5X13-X0X1-6S7UWL^FS^FO200,105^AD^FDCJHJH035^FS^FO10,195^BY3,2,86^B3,,,N^FDCJHJH035^FS^FO200,105^AD^FDPP^FS^FO200,105^AD^FD1^FS^FO200,105^AD^FDP990^FS^FO200,105^AD^FDHOLD^FS^PQ1^XZ"

        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TNR_FAIL_LABEL HDR=(LOTmgr,%s,TNR_FAIL_LABEL) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                                "LOTID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "SIMAX";
    }else if( TCmd == "PMSMAP" )
    {

        TMsg.Format((char *)"PMSMAP HDR=(LOTmgr,%s,PMSMAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC " ,
                        m_TRvMosSubjectHDR.GetPrintString(),
                       TFilter.GetArg((char *)"EQPID"),
                       ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID") );
        
        AddMsg( TFilter, TMsg, "LOTID"                  , "LOTID");
        AddMsg( TFilter, TMsg, "WAFERID"                , "WAFERID");
        AddMsg( TFilter, TMsg, "TYPE"                   , "TYPE");
        AddMsg( TFilter, TMsg, "OBJECTID"               , "OBJECTID");
        AddMsg( TFilter, TMsg, "MAX_X_POSN"             , "MAX_X_POSN");             
        AddMsg( TFilter, TMsg, "MAX_Y_POSN"             , "MAX_Y_POSN");            
        AddMsg( TFilter, TMsg, "CELL_STATUS_SEG_LIST"   , "CELL_STATUS_SEG_LIST");
        AddMsg( TFilter, TMsg, "WAFERLIST"              , "WAFERLIST");   
        AddMsg( TFilter, TMsg, "ATTR"                   , "ATTR");             
        AddMsg( TFilter, TMsg, "BASE_REORG_YN"          , "BASE_REORG_YN");
        AddMsg( TFilter, TMsg, "PMS_ATTR"               , "PMS_ATTR");         
        AddMsg( TFilter, TMsg, "ISSUE_YN"               , "ISSUE_YN");              
        AddMsg( TFilter, TMsg, "BASE_WORK_ANGLE"        , "BASE_WORK_ANGLE");
        AddMsg( TFilter, TMsg, "BASE_REVERSE_YN"        , "BASE_REVERSE_YN");
        AddMsg( TFilter, TMsg, "BASE_REVERSE_AXIS"      , "BASE_REVERSE_AXIS");
        AddMsg( TFilter, TMsg, "CORE_WORK_ANGLE"        , "CORE_WORK_ANGLE");
        AddMsg( TFilter, TMsg, "CORE_REVERSE_YN"        , "CORE_REVERSE_YN");
        AddMsg( TFilter, TMsg, "CORE_REVERSE_AXIS"      , "CORE_REVERSE_AXIS");
        AddMsg( TFilter, TMsg, "BOATID"                 , "BOATID");
        AddMsg( TFilter, TMsg, "CHIP_ID_LIST"           , "CHIP_ID_LIST");
    AddMsg( TFilter, TMsg, "ACTION"                 , "ACTION");

        TMesTarget = "SIMAX";

    }else if( TCmd == "PMSSCRAP" )
    {
        //  CMD=PMSSCRAP PROCESS=%s EQPID=%s OPERID=%s WAFERID=%s PMSSCRAPINFO=(%s) 

        if( CheckPara(TFilter, "WAFERID"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "PMSSCRAPINFO"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }        

        TMsg.Format((char *)"PMSSCRAP HDR=(LOTmgr,%s,PMSSCRAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                                "WAFERID=%s PMSSCRAPINFO=(%s) ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"WAFERID"),
                    TFilter.GetArg((char *)"PMSSCRAPINFO") );

        TMesTarget = "SIMAX";
    }else if( TCmd == "PMSMAPATTR" )
    {
        //  CMD=PMSMAPATTR PROCESS=%s EQPID=%s OPERID=%s WAFERID=%s TYPE=%s

        if( CheckPara(TFilter, "WAFERID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PMSMAPATTR HDR=(LOTmgr,%s,PMSMAPATTR) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                                "WAFERID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"WAFERID") );

        AddMsg( TFilter, TMsg, "TYPE"           , "TYPE" );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",       "(",    ")"     ); 

        TMesTarget = "SIMAX";
    }else if( TCmd == "PMSDISPLAY" )
    {
        //  CMD=PMSDISPLAY PROCESS=%s EQPID=%s OPERID=%s BASE_OBJECT_ID=%s BASE_STATUS_YN=%s

        if( CheckPara(TFilter, "BASE_OBJECT_ID" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PMSDISPLAY HDR=(LOTmgr,%s,PMSDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                                "BASE_OBJECT_ID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"BASE_OBJECT_ID") );

        AddMsg( TFilter, TMsg, "BASE_STATUS_YN"           , "BASE_STATUS_YN" );

        TMesTarget = "SIMAX";
    }else if( TCmd == "REELMAP" )
    {
        //  CMD=REELMAP PROCESS=%s EQPID=%s OPERID=%s LOTID=%s OBJECTID=%s

        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "OBJECTID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"REELMAP HDR=(LOTmgr,%s,PMSMAP) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                                "LOTID=%s OBJECTID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"OBJECTID") );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "NEWPARTID" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"NEWPARTID HDR=(LOTmgr,%s,NEWPARTID) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO"                        );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SETDISPLAY" )     
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SETSEG"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SETDISPLAY HDR=(LOTmgr,%s,SETDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s SETSEG=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SETSEG"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CONSMDISPLAY" )     
    {
        if( CheckPara(TFilter, "CONSMID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CONSMDISPLAY HDR=(LOTmgr,%s,CONSMDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"));

        AddMsg( TFilter, TMsg, "ATTRIBUTE"        , "ATTRIBUTE"                 );
        AddMsg( TFilter, TMsg, "REQTYPE"          , "REQTYPE"                   ); 

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "BOM_MERGEMATLOT" )     
    {
        if( CheckPara(TFilter, "CONSMID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"BOM_MERGEMATLOT HDR=(LOTmgr,%s,BOM_MERGEMATLOT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"));

        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "SUBCONSMID"     , "SUBCONSMID"                  );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "BOM_MODIFYATTRMATLOT" )     
    {
        if( CheckPara(TFilter, "CONSMID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ATTR"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"BOM_MODIFYATTRMATLOT HDR=(LOTmgr,%s,BOM_MODIFYATTRMATLOT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ATTR=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"),
                TFilter.GetArg((char *)"ATTR"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "BOM_SPLITMATLOT" )     
    {
        if( CheckPara(TFilter, "CONSMID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"BOM_SPLITMATLOT HDR=(LOTmgr,%s,BOM_SPLITMATLOT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"));
        AddMsg( TFilter, TMsg, "SPLITCONSMID"       , "SPLITCONSMID"            );
        AddMsg( TFilter, TMsg, "SPLITQTY"           , "SPLITQTY"                );
        AddMsg( TFilter, TMsg, "ACTIVITY"           , "ACTIVITY"                );
        AddMsg( TFilter, TMsg, "BOMOPTION"          , "BOMOPTION"               ); 

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "BOM_SCRAPMATLOT" )     
    {
        if( CheckPara(TFilter, "CONSMID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"BOM_SCRAPMATLOT HDR=(LOTmgr,%s,BOM_SCRAPMATLOT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"));
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                         );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "REASON"         , "REASON"                      );
        AddMsg( TFilter, TMsg, "BOMOPTION"      , "BOMOPTION"                   );
        AddMsg( TFilter, TMsg, "SCRAPCODE"      , "SCRAPCODE"                   ); // 2023.06.07 AJS : Add
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",       "(",    ")"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MATSEND" )     
    {
        if( CheckPara(TFilter, "CONSMID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MATSEND HDR=(LOTmgr,%s,MATSEND) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"));
        AddMsg( TFilter, TMsg, "CONSMPRODID"    , "CONSMPRODID"                 );
        AddMsg( TFilter, TMsg, "LOCATION"       , "LOCATION"                    );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                         );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "REASON"         , "REASON"                      );
        AddMsg( TFilter, TMsg, "BOMOPTION"      , "BOMOPTION"                   );
        AddMsg( TFilter, TMsg, "LOTTYPE"        , "LOTTYPE"                     );
        AddMsg( TFilter, TMsg, "VENDORID"       , "VENDORID"                    );
        AddMsg( TFilter, TMsg, "LINE"           , "LINE"                        );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",       "(",    ")"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "BOM_BONUSMATLOT" )     
    {
        if( CheckPara(TFilter, "CONSMID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"BOM_BONUSMATLOT HDR=(LOTmgr,%s,BOM_BONUSMATLOT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "CONSMID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CONSMID"));
        AddMsg( TFilter, TMsg, "LOCATION"       , "LOCATION"                    );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                         );
        AddMsg( TFilter, TMsg, "ACTIVITY"       , "ACTIVITY"                    );
        AddMsg( TFilter, TMsg, "BOMOPTION"      , "BOMOPTION"                   );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TOOLCHANGE" )     
    {
        if( CheckPara(TFilter, "TOOLID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TOOLCHANGE HDR=(LOTmgr,%s,TOOLCHANGE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "TOOLID=\"%s\" ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"TOOLID"));
        AddMsg( TFilter, TMsg, "ACTION"       , "ACTION"                        );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PRODLABELAUTO" )     
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PRODLABELAUTO HDR=(LOTmgr,%s,PRODLABELAUTO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));
        AddMsg( TFilter, TMsg, "PRINTCNT"       , "PRINTCNT"                    );
        AddMsg( TFilter, TMsg, "LABEL_CODE"     , "LABEL_CODE"                  );
        AddMsg( TFilter, TMsg, "RIBBON_CODE"    , "RIBBON_CODE"                 );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSDCCS" )     
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSDCCS HDR=(LOTmgr,%s,SSDCCS) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));
        AddMsg( TFilter, TMsg, "PRODUCT"        , "PRODUCT"                     );
        AddMsg( TFilter, TMsg, "TYPE"           , "TYPE"                        );
        AddMsg( TFilter, TMsg, "MATS"           , "MATS",       "(",    ")"     );
        AddMsg( TFilter, TMsg, "MATAREA"        , "MATAREA"                     );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                    );// 2022.12.13 Add
        AddMsg( TFilter, TMsg, "EQPID"          , "EQPID"                       );// 2022.12.13 Add
        AddMsg( TFilter, TMsg, "PRODUCT"        , "PRODUCT"                     );// 2022.12.13 Add
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"                     );// 2022.12.13 Add
        AddMsg( TFilter, TMsg, "QUANTITY"       , "QUANTITY"                    );// 2022.12.13 Add

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSDCASECCS" )     // 2022.12.13 AJS Add
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSDCASECCS HDR=(LOTmgr,%s,SSDCASECCS) LOTID=%s OPERID=%s FMATS=(%s) BMATS=(%s) TYPE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"FMATS"),
                TFilter.GetArg((char *)"BMATS"),
                TFilter.GetArg((char *)"TYPE"));
         TMesTarget = "SIMAX";
    }
    else if( TCmd == "CARR_USAGE_MANAGE" )     
    {
        if( CheckPara(TFilter, "CARRID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CARRTYPE"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ACTIONTYPE" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STATUS"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "LINEID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CARR_USAGE_MANAGE  HDR=(LOTmgr,%s,CARR_USAGE_MANAGE ) EQPID=%s OPERID=%s CARRID=%s CARRTYPE=%s ACTIONTYPE=%s STATUS=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"), ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CARRID"), TFilter.GetArg((char *)"CARRTYPE"), TFilter.GetArg((char *)"ACTIONTYPE"), TFilter.GetArg((char *)"STATUS") ) ;

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "WS_SPLITTKOUT_REQ" ) // 2018.02.12 AJS : Split TkOut ADD
    {
        //WS_SPLITTKOUT_REQ LOTID=SIMAXAAA OPERID=AUTO EQPID=AB01 SPLITWFINFO=(SIMAX-001,SIMAX-003) REQ_SYSTEM=TC
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SPLITWFINFO", TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WS_SPLITTKOUT_REQ HDR=(LOTmgr,%s,WS_SPLITTKOUT_REQ) LOTID=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "SPLITWFINFO=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SPLITWFINFO") );
        //2018.07.31 AJS : SPLITTYPE �߰�
        AddMsg( TFilter, TMsg, "SPLITTYPE"      , "SPLITTYPE"       );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "DIRECTSHIP" )   
    {
        if( CheckPara(TFilter, "LOTID"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "CARRIERID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        //DIRECTSHIP HDR=(AA,AA,AA) LOTID=#LotId# OPERID=#OperId# FROMAREA=CAS2 TOAREA=CHTV TOSITE=O2 CARRIERID=#CarrierId# ACTIVITY=SHIP_CAS2_O2
        TMsg.Format((char *)"DIRECTSHIP HDR=(LOTmgr,%s,DIRECTSHIP ) OPERID=%s LOTID=%s FROMAREA=CAS2 TOAREA=CHTV TOSITE=O2 CARRIERID=%s ACTIVITY=SHIP_CAS2_O2 REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"), TFilter.GetArg((char *)"CARRIERID")) ;

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SPLITTKOUT" )   
    {
        if( CheckPara(TFilter, "LOTID"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SPLITQTY"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SPLITTKOUT HDR=(LOTmgr,%s,SPLITTKOUT ) OPERID=%s EQPID=%s LOTID=%s SPLITQTY=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SPLITQTY"));

        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"       );
        AddMsg( TFilter, TMsg, "SPLITTYPE"      , "SPLITTYPE"       );

        TMesTarget = "SIMAX";
    }
    else if ( TCmd == "CIS_WAFER_LABEL" )      
    {
        if( CheckPara(TFilter, "LOTID"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "WAFER_SEQ"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }


        TMsg.Format((char *)"CIS_WAFER_LABEL HDR=(LOTmgr,%s,CIS_WAFER_LABEL ) OPERID=%s LOTID=%s WAFER_SEQ=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"), TFilter.GetArg((char *)"WAFER_SEQ"));

        TMesTarget = "SIMAX";
    }
    else if ( TCmd == "SAWNWAFERMAP" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "WAFERID"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "QTY"            , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SAWNWAFERMAP HDR=(LOTmgr,%s,SAWNWAFERMAP) LINEID=%s OPERID=%s EQPID=%s REQ_SYSTEM=TC "
                            "LOTID=%s WAFERID=%s QTY=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"WAFERID"),
                TFilter.GetArg((char *)"QTY")
            );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PARTCHG_REQ" )   
    {                                  
                                       
        if( CheckPara(TFilter, "LOTID"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        //PARTCHG_REQ HDR=(AA,AA,AA) LOTID=SIMAXAAA EQPID=AB01 REQ_SYSTEM=TC
        TMsg.Format((char *)"PARTCHG_REQ HDR=(LOTmgr,%s,PARTCHG_REQ ) LOTID=%s EQPID=%s CHG_FLAG=%s EXPLOTID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"EQPID"), 
                TFilter.GetArg((char *)"CHG_FLAG"),
                TFilter.GetArg((char *)"EXPLOTID"));

        TMesTarget = "WORKMAN";
    }else if ( TCmd == "ADJUST_COMP_BIN_QTY" )  
    {
        if( CheckPara(TFilter, "COMPONENTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"ADJUST_COMP_BIN_QTY HDR=(LOTmgr,%s,ADJUST_COMP_BIN_QTY) LINEID=%s OPERID=%s EQPID=%s REQ_SYSTEM=TC "
                            "COMPONENTID=%s GOOD_BIN_QTY=%s LOW_BIN_QTY=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"COMPONENTID"),
                TFilter.GetArg((char *)"GOOD_BIN_QTY"),
                TFilter.GetArg((char *)"LOW_BIN_QTY")
            );

        TMesTarget = "SIMAX";
    }
    else if ( TCmd == "COMPMAPCHG" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"COMPMAPCHG HDR=(LOTmgr,%s,COMPMAPCHG) LINEID=%s OPERID=%s EQPID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"));

        AddMsg( TFilter, TMsg, "NEW_COMP_INFO_LIST"           , "NEW_COMP_INFO_LIST",       "(",    ")"     );

        TMesTarget = "SIMAX";
    }else if( TCmd == "FRAMEMODATTR" )          
    {
        TMsg.Format((char *)"FRAMEMODATTR HDR=(LOTmgr,%s,FRAMEMODATTR) LINEID=%s EQPID=%s OPERID=AUTO FRAMEID=%s ATTR=(%s) REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"LINE"),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"FRAMEID"),
                    TFilter.GetArg((char *)"ATTR"));

        TMesTarget = "SIMAX";
    }else if ( TCmd == "PMSMAPACT" )        
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "WAFERID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PMSMAPACT HDR=(LOTmgr,%s,PMSMAPACT) LINEID=%s EQPID=%s OPERID=%s LOTID=%s WAFERID=%s TYPE=%s ISSUE_YN=%s REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"LINE"),
                    TFilter.GetArg((char *)"EQPID"),
                    ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"WAFERID"),
                    ( TFilter.GetArg((char *)"TYPE")  == NULL ) ? "WAFER" : TFilter.GetArg((char *)"TYPE"),
                    ( TFilter.GetArg((char *)"ISSUE_YN")  == NULL ) ? "Y" : TFilter.GetArg((char *)"ISSUE_YN"));

        AddMsg( TFilter, TMsg, "BASE_WORK_ANGLE"      , "BASE_WORK_ANGLE"       );
        AddMsg( TFilter, TMsg, "BASE_REVERSE_YN"      , "BASE_REVERSE_YN"       );
        AddMsg( TFilter, TMsg, "BASE_REVERSE_AXIS"    , "BASE_REVERSE_AXIS"     );
        AddMsg( TFilter, TMsg, "BASE_MAX_X_POSN"      , "BASE_MAX_X_POSN"       );
        AddMsg( TFilter, TMsg, "BASE_MAX_Y_POSN"      , "BASE_MAX_Y_POSN"       );
        AddMsg( TFilter, TMsg, "CORE_WORK_ANGLE"      , "CORE_WORK_ANGLE"       );
        AddMsg( TFilter, TMsg, "CORE_REVERSE_YN"      , "CORE_REVERSE_YN"       );
        AddMsg( TFilter, TMsg, "CORE_REVERSE_AXIS"    , "CORE_REVERSE_AXIS"     );
        AddMsg( TFilter, TMsg, "CORE_MAX_X_POSN"      , "CORE_MAX_X_POSN"       );
        AddMsg( TFilter, TMsg, "CORE_MAX_Y_POSN"      , "CORE_MAX_Y_POSN"       );

        TMesTarget = "SIMAX";
    }

    else if( TCmd == "GET_SIMAXDATA_OLD" )
    {
        if( CheckPara(TFilter, "SQL"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "COLUMNCOUNT" , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "BINDCOUNT"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "VALUE"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"GET_SIMAXDATA HDR=(LOTmgr,%s,GET_SIMAXDATA) PROCESS=%s LINE=%s EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "SQL=\"%s\" COLUMNCOUNT=%s BINDCOUNT=%s VALUE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PROCESS"),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SQL"),
                TFilter.GetArg((char *)"COLUMNCOUNT"),
                TFilter.GetArg((char *)"BINDCOUNT"),
                TFilter.GetArg((char *)"VALUE") );

        AddMsg( TFilter, TMsg, "SQL_DESC"   , "SQL_DESC", "\"", "\"" ); 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PCBDISPLAY" )
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PCBDISPLAY HDR=(LOTmgr,%s,PCBDISPLAY) PROCESS=%s LINE=%s EQPID=%s LOTID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PROCESS"),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        AddMsg( TFilter, TMsg, "STEP_SEQ"       , "STEP_SEQ"                        );
        AddMsg( TFilter, TMsg, "PCB_LOTID"      , "PCB_LOTID"                       );
        AddMsg( TFilter, TMsg, "SPLITLOTID"     , "SPLITLOTID"                      );
        AddMsg( TFilter, TMsg, "MODE"           , "MODE"                            );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTRESRVINFO" )
    {
        TMsg.Format((char *)"LOTRESRVINFO HDR=(LOTmgr,%s,LOTRESRVINFO) PROCESS=%s LINE=%s EQPID=%s OPERID=%s MODE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PROCESS"),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"MODE"));

        AddMsg( TFilter, TMsg, "SUBCMDS"       , "SUBCMDS"                        );
        AddMsg( TFilter, TMsg, "PCBSERIAL"     , "PCBSERIAL"                      );
        AddMsg( TFilter, TMsg, "PCB_LOTID"     , "PCB_LOTID"                      );
        AddMsg( TFilter, TMsg, "STEP"          , "STEP"                           );
        AddMsg( TFilter, TMsg, "LOTID"         , "LOTID"                          );
        AddMsg( TFilter, TMsg, "RESRVID"       , "RESRVID"                        );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MODAUTOISSUE" )
    {
        if( CheckPara(TFilter, "PCBLOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MODAUTOISSUE HDR=(LOTmgr,%s,MODAUTOISSUE) PROCESS=%s LINE=%s EQPID=%s PCBLOTID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PROCESS"),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"PCBLOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_SPLIT" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_SPLIT HDR=(LOTmgr,%s,IMS_SPLIT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SPLITQTY"       , "SPLITQTY"                        );
        AddMsg( TFilter, TMsg, "TYPE"           , "TYPE"                            );
        AddMsg( TFilter, TMsg, "SPLITGUBUN"     , "SPLITGUBUN"                      );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "MERGEPROCID"    , "MERGEPROCID"                     );
        AddMsg( TFilter, TMsg, "MERGEINSTNO"    , "MERGEINSTNO"                     );
        AddMsg( TFilter, TMsg, "MERGERECIPEID"  , "MERGERECIPEID"                   );
        AddMsg( TFilter, TMsg, "SPLITLOTID"     , "SPLITLOTID"                      );
        AddMsg( TFilter, TMsg, "SBLFLAG"        , "SBLFLAG"                         );
        AddMsg( TFilter, TMsg, "SPLITTYPE"      , "SPLITTYPE"                       );
        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO",        "(",    ")"     );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "\"",   "\""    );
        AddMsg( TFilter, TMsg, "SPLITWFINFO"    , "SPLITWFINFO",    "(",    ")"     );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "RETEST_YN"      , "RETEST_YN"                       );
        AddMsg( TFilter, TMsg, "SPLITTIMES"     , "SPLITTIMES"                      );
        AddMsg( TFilter, TMsg, "WAFERQTY"       , "WAFERQTY"                        );
        AddMsg( TFilter, TMsg, "LAST_LOT_FIX_YN", "LAST_LOT_FIX_YN"                 );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_TKIN" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_TKIN HDR=(LOTmgr,%s,IMS_TKIN) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "EQPTYPE"        , "EQPTYPE"                         );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"     ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "LINE"           , "LINE"                            );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                             );
        AddMsg( TFilter, TMsg, "BATCHID"        , "BATCHID"                         );
        AddMsg( TFilter, TMsg, "MERGESTEP"      , "MERGESTEP"                       );
        AddMsg( TFilter, TMsg, "MULTIEQPINFO"   , "MULTIEQPINFO",   "(",    ")"     );
        AddMsg( TFilter, TMsg, "ATTRINFO"       , "ATTRINFO"    ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR"        ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "EQPSTATUS"      , "EQPSTATUS"                       );
        AddMsg( TFilter, TMsg, "SOCKETINFO"     , "SOCKETINFO"  ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "MATINFO"        , "MATINFO"     ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "REQTYPE"        , "REQTYPE"                         );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "FRMCONV"        , "FRMCONV"                         );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MOD_RESERVEEQP_REQ" )
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MOD_RESERVEEQP_REQ HDR=(LOTmgr,%s,MOD_RESERVEEQP_REQ) PROCESS=%s LINE=%s EQPID=%s LOTID=%s INLINEEQP=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PROCESS"),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"INLINEEQP"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));
        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "IMS_LOTDISPLAY" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_LOTDISPLAY HDR=(LOTmgr,%s,IMS_LOTDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "ATTRIBUTE"      , "ATTRIBUTE"       );
        AddMsg( TFilter, TMsg, "NEXTSTPS"       , "NEXTSTPS"        );
        AddMsg( TFilter, TMsg, "MCPINFO"        , "MCPINFO"         );
        AddMsg( TFilter, TMsg, "BAKEREMAINTIME" , "BAKEREMAINTIME"  );
        AddMsg( TFilter, TMsg, "STEPSKIPINFO"   , "STEPSKIPINFO"    );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"       );
        AddMsg( TFilter, TMsg, "PLASMAFLAG"     , "PLASMAFLAG"      );
        AddMsg( TFilter, TMsg, "TRAY_INFO"      , "TRAY_INFO"       );
        AddMsg( TFilter, TMsg, "REQTYPE"        , "REQTYPE"         );
        AddMsg( TFilter, TMsg, "WAFER_INFO_YN"  , "WAFER_INFO_YN"   );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PMS_SET" )
    {
        if( CheckPara(TFilter, "PCBSRLNO"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PMS_SET HDR=(LOTmgr,%s,PMS_SET) PROCESS=%s LINE=%s EQP_ID=%s PCB_SRL_NO=%s PU=%s MODE=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PROCESS"),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"PCBSRLNO"),
                TFilter.GetArg((char *)"PU"),
                TFilter.GetArg((char *)"MODE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));
        AddMsg( TFilter, TMsg, "LOT_ID"          , "LOT_ID"            );
        AddMsg( TFilter, TMsg, "PCB_LOT_ID"      , "PCB_LOT_ID"        );
        AddMsg( TFilter, TMsg, "LABEL_SRL_INFO"  , "LABEL_SRL_INFO",        "(",   ")"    );
        AddMsg( TFilter, TMsg, "PCB_MAT_CODE"    , "PCB_MAT_CODE"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_MERGE" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_MERGE HDR=(LOTmgr,%s,IMS_MERGE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SUBLOTID"       , "SUBLOTID",           "(",    ")" );
        AddMsg( TFilter, TMsg, "MERGETIMES"     , "MERGETIMES"                      );
        AddMsg( TFilter, TMsg, "VALIDATION"     , "VALIDATION"                      );
        AddMsg( TFilter, TMsg, "FRAMEMERGEFLAG" , "FRAMEMERGEFLAG"                  );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR",           "(",    ")"     );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "FRMCONV"        , "FRMCONV"                         );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_MODULETKOUT" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_MODULETKOUT HDR=(LOTmgr,%s,IMS_MODULETKOUT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "SCRAPINFO"      , "SCRAPINFO"   ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "METALINFO"      , "METALINFO"   ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "BININFO"        , "BININFO"     ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "WAFERINFO"      , "WAFERINFO"   ,   "(",    ")"     );
        AddMsg( TFilter, TMsg, "DPKGQTY"        , "DPKGQTY"                         );
        AddMsg( TFilter, TMsg, "CHILDLOT"       , "CHILDLOT"                        );
        AddMsg( TFilter, TMsg, "BATCHLOTFLAG"   , "BATCHLOTFLAG"                    );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );
        AddMsg( TFilter, TMsg, "ATTR"           , "ATTR"   ,        "(",    ")"     );
        AddMsg( TFilter, TMsg, "STEPSKIP"       , "STEPSKIP"                        );
        AddMsg( TFilter, TMsg, "HOLDCODE"       , "HOLDCODE"                        );
        AddMsg( TFilter, TMsg, "WORKORDERDEL"   , "WORKORDERDEL"                    );
        AddMsg( TFilter, TMsg, "NEXTSTEPSEQ"    , "NEXTSTEPSEQ"                     );
        AddMsg( TFilter, TMsg, "STEPSEQ"        , "STEPSEQ"                         );
        AddMsg( TFilter, TMsg, "PARTID"         , "PARTID"                          );
        AddMsg( TFilter, TMsg, "EQTYPE"         , "EQTYPE"                          );
        AddMsg( TFilter, TMsg, "GOODQTY"        , "GOODQTY"                         );
        AddMsg( TFilter, TMsg, "QTY"            , "QTY"                             );
        AddMsg( TFilter, TMsg, "REWORK_STEP"    , "REWORK_STEP"                     );
        AddMsg( TFilter, TMsg, "REJOIN_STEP"    , "REJOIN_STEP"                     );
        AddMsg( TFilter, TMsg, "PCBINFO"        , "PCBINFO"                         );
        AddMsg( TFilter, TMsg, "CARRIERID"      , "CARRIERID"                       );
        AddMsg( TFilter, TMsg, "FRMCONV"        , "FRMCONV"                         );
        AddMsg( TFilter, TMsg, "BADCODE"        , "BADCODE"         ,   "(",    ")" );
        AddMsg( TFilter, TMsg, "BADQTY"         , "BADQTY"          ,   "(",    ")" );
        AddMsg( TFilter, TMsg, "SCRAPPRODSERIAL", "SCRAPPRODSERIAL" ,   "(",    ")" );
            AddMsg( TFilter, TMsg, "KEEP_SETID"     , "KEEP_SETID"                      );
            AddMsg( TFilter, TMsg, "SBLYN"          , "SBLYN"                           );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_EQPDISPLAY" )
    {
        if( CheckPara(TFilter, "EQPID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_EQPDISPLAY HDR=(LOTmgr,%s,IMS_EQPDISPLAY) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_MODATTR" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "ATTR"       , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_MODATTR HDR=(LOTmgr,%s,IMS_MODATTR) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ATTR=(%s) ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"ATTR") );

        AddMsg( TFilter, TMsg, "ATTRNAME"       , "ATTRNAME"                        );
        AddMsg( TFilter, TMsg, "ATTRVALUE"      , "ATTRVALUE"                       );
        AddMsg( TFilter, TMsg, "BATCHID"        , "BATCHID"                         );
        AddMsg( TFilter, TMsg, "FLAG"           , "FLAG"                            );
        AddMsg( TFilter, TMsg, "STARTPOSITION"  , "STARTPOSITION"                   );
        AddMsg( TFilter, TMsg, "ENFORCE"        , "ENFORCE"                         );
        AddMsg( TFilter, TMsg, "COMMENT"        , "COMMENT",        "\"",   "\""    );
        AddMsg( TFilter, TMsg, "REQTYPES"       , "REQTYPES"                        );
        AddMsg( TFilter, TMsg, "TXNTIME"        , "TXNTIME"                         );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SBLBYEDSCHIP" )
    {
        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SBLBYEDSCHIP HDR=(LOTmgr,%s,SBLBYEDSCHIP) LINE=%s EQPID=%s SBLINFO=(%s) LOTID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"SBLINFO"),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IMS_PRODLABEL" )
    {
        if( CheckPara(TFilter, "LOTID"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IMS_PRODLABEL HDR=(LOTmgr,%s,IMS_PRODLABEL) LINE=%s EQPID=%s LOTID=%s PRINTCNT=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"PRINTCNT"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        AddMsg( TFilter, TMsg, "LABEL_CODE"       , "LABELCODE"                        );
        AddMsg( TFilter, TMsg, "RIBBON_CODE"      , "RIBBONCODE"                       );
        AddMsg( TFilter, TMsg, "SUBCMDS"          , "SUBCMDS"                          );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "WAFER_SHEET_LABEL" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WAFER_SHEET_LABEL HDR=(LOTmgr,%s,WAFER_SHEET_LABEL) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                                "LOTID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "SIMAX";
    }else if ( TCmd == "PMSMAPDEL" )
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "OBJECT_LIST"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PMSMAPDEL HDR=(LOTmgr,%s,PMSMAPDEL) EQPID=%s OPERID=AUTO REQ_SYSTEM=TC "
                                "LOTID=%s OBJECT_LIST=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"LOTID"),
                    TFilter.GetArg((char *)"OBJECT_LIST"));

        TMesTarget = "SIMAX";

    }
    else if( TCmd == "MAT_ISSUE" )
    {
        //if( CheckPara(TFilter, "PCBSERIAL"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MAT_ISSUE HDR=(LOTmgr,%s,MAT_ISSUE) EQPID=%s OPERID=%s PCBLOTID=%s REQ_SYSTEM=TC "
                            "PCBSERIAL=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PCBLOTID"),
                TFilter.GetArg((char *)"PCBSERIAL"));

        AddMsg( TFilter, TMsg, "AUTO"        , "AUTO"                     );


        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PMS_SETDATA" )     
    {
        TMsg.Format((char *)"PMS_SETDATA HDR=(LOTmgr,%s,PMS_SETDATA) EQPID=%s OPERID=%s MODE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),        
                TFilter.GetArg((char *)"MODE"));

        AddMsg( TFilter, TMsg, "BARCODE_SRLNO"        , "BARCODE_SRLNO"                     );
        AddMsg( TFilter, TMsg, "SPLIT_LOT_ID"         , "SPLIT_LOT_ID"                      );
        AddMsg( TFilter, TMsg, "ATTR"                 , "ATTR",             "(",    ")"     );
        AddMsg( TFilter, TMsg, "BEFORE_PCBSRLNO"      , "BEFORE_PCBSRLNO"                   );
        AddMsg( TFilter, TMsg, "SUBCMDS"              , "SUBCMDS"                           );
        AddMsg( TFilter, TMsg, "MERGE_LOT_ID"         , "MERGE_LOT_ID"                      );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CREATE_MULTILOT" )     
    {
        if( CheckPara(TFilter, "GROUPID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CREATE_MULTILOT HDR=(LOTmgr,%s,CREATE_MULTILOT) EQPID=%s OPERID=%s GROUPTYPE=%s MULTILOTS=%s REQ_SYSTEM=TC "
                            "GROUPID=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),        
                TFilter.GetArg((char *)"GROUPTYPE"),
                TFilter.GetArg((char *)"MULTILOTS"),
                TFilter.GetArg((char *)"GROUPID"));

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "AUTOPARTCHANGE_REQ" )     
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"AUTOPARTCHANGE_REQ HDR=(LOTmgr,%s,AUTOPARTCHANGE_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CHKPGMINFO" )     
    {
        if( CheckPara(TFilter, "PART_NO"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SRLNO"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHKPGMINFO HDR=(LOTmgr,%s,CHKPGMINFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "SRLNO=%s PART_NO=%s " ,
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SRLNO"),
                TFilter.GetArg((char *)"PART_NO"));

        AddMsg( TFilter, TMsg, "PCB_SIDE"        , "PCB_SIDE"     );
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PCB_CLEAN_REQ" )     
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PCB_CLEAN_REQ HDR=(LOTmgr,%s,PCB_CLEAN_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EQP_CHG_CONFIRM_REQ" )     
    {
        TMsg.Format((char *)"EQP_CHG_CONFIRM_REQ HDR=(LOTmgr,%s,EQP_CHG_CONFIRM_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "SMTREMAININFO" )     
    {
        if( CheckPara(TFilter, "RESRV_SEQ_ID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SMTREMAININFO HDR=(LOTmgr,%s,SMTREMAININFO) EQPID=%s OPERID=%s STEPSEQ=%s MODE=%s REQ_SYSTEM=TC "
                            "RESRV_SEQ_ID=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"STEPSEQ"),
                TFilter.GetArg((char *)"MODE"),
                TFilter.GetArg((char *)"RESRV_SEQ_ID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PCBNANDMAP" )     
    {
        TMsg.Format((char *)"PCBNANDMAP HDR=(LOTmgr,%s,PCBNANDMAP) PCBSERIAL=%s PCBSIDE=%s NANDSRLNO=%s EQPID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PCBSERIAL"),
                TFilter.GetArg((char *)"PCBSIDE"),
                TFilter.GetArg((char *)"NANDSRLNO"),
                TFilter.GetArg((char *)"EQPID"));
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PMS_SETMAP" )     
    {
        TMsg.Format((char *)"PMS_SETMAP HDR=(LOTmgr,%s,PMS_SETMAP) BARCODE_SRLNO=%s OPERID=%s FUNCTION=%s MAPINFO=%s SIDE=%s EQPID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"BARCODE_SRLNO"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"FUNCTION"),
                TFilter.GetArg((char *)"MAPINFO"),
                TFilter.GetArg((char *)"SIDE"),
                TFilter.GetArg((char *)"EQPID"));

        AddMsg( TFilter, TMsg, "SCRAPINFO", "SCRAPINFO", "(", ")" );
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MULTI_TKIN_REQ" )     
    {
        TMsg.Format((char *)"MULTI_TKIN_REQ HDR=(LOTmgr,%s,MULTI_TKIN_REQ) EQPID=%s LOTID=%s SETID=%s TOSETID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SETID"),
                TFilter.GetArg((char *)"TOSETID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));
 
        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "CHKLOWYIELD" )     
    {
        TMsg.Format((char *)"CHKLOWYIELD HDR=(LOTmgr,%s,CHKLOWYIELD) EQPID=%s OPERID=%s LOTID=%s SETID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SETID"));

        AddMsg( TFilter, TMsg, "SCRAPINFO"      , "SCRAPINFO"   ,   "(",    ")"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTCLOSE" )     
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTCLOSE HDR=(LOTmgr,%s,LOTCLOSE) EQPID=%s OPERID=%s GATE=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"GATE"),
                TFilter.GetArg((char *)"LOTID"));

		    AddMsg( TFilter, TMsg, "TYPE"    , "TYPE"     );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "LOTXCLOSE" )     
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTXCLOSE HDR=(LOTmgr,%s,LOTXCLOSE) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));

        TMesTarget = "SIMAX";
    }   
    else if( TCmd == "MOD_SBOXMAKE" )     
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MOD_SBOXMAKE HDR=(LOTmgr,%s,MOD_SBOXMAKE) EQPID=%s PACKGATE=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"PACKGATE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYDISSOLVE" )     
    {
        TMsg.Format((char *)"TRAYDISSOLVE HDR=(LOTmgr,%s,TRAYDISSOLVE) EQPID=%s OPERID=%s SUBACTTYPE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SUBACTTYPE"));

        AddMsg( TFilter, TMsg, "SBOXID"                   , "SBOXID"                              );
        AddMsg( TFilter, TMsg, "LOTID"                    , "LOTID"                               );
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SERIALCHECK" )     // 2022.05.25 AJS : Cqac Cancel
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SERIALCHECK HDR=(LOTmgr,%s,SERIALCHECK) LOTID=%s OPERID=%s SERIALNO=(%s) CHKCNT=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"CQAC_INFO"),
                TFilter.GetArg((char *)"CQAC_CNT"));
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYMAPPING" )     // 2022.06.27 AJS : Cap & Label Add
    {
        if( CheckPara(TFilter, "LOTID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        //20250721 CGH : AVI ���� SVID 2015 JEDEC_SRLNO �߰� MOS�� ���� �޼��� FORMAT�� MODULE_INFO�� ���� 
        TMsg.Format((char *)"TRAYMAPPING HDR=(LOTmgr,%s,TRAYMAPPING) EQPID=%s LOTID=%s OPERID=%s PROD_SRLNO=(%s) JEDEC_SRLNO=(%s) CHKCNT=%s ACTTYPE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PROD_SRLNO"),
                TFilter.GetArg((char *)"JEDEC_SRLNO"),
                TFilter.GetArg((char *)"SERIAL_CNT"),
                TFilter.GetArg((char *)"ACTTYPE"));
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYLABEL" )     // 2022.06.27 AJS : Cap & Label Add
    {
        if( CheckPara(TFilter, "TRAYID"    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TRAYLABEL HDR=(LOTmgr,%s,TRAYLABEL) EQPID=%s TRAYID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"TRAYID"),
                TFilter.GetArg((char *)"OPERID"));
 
        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYLABELINFO" )     
    {
        if( CheckPara(TFilter, "TRAYID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TRAYLABELINFO HDR=(LOTmgr,%s,TRAYLABELINFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "TRAYID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"TRAYID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TRAYSBOXMAPPING" )     
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TRAYSBOXMAPPING HDR=(LOTmgr,%s,TRAYSBOXMAPPING) EQPID=%s SBOXTRAYID=(%s) OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"SBOXTRAYID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MOD_SBOXLABEL" )     
    {
        if( CheckPara(TFilter, "SBOXID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MOD_SBOXLABEL HDR=(LOTmgr,%s,MOD_SBOXLABEL) EQPID=%s PACKGATE=%s OPERID=%s REQ_SYSTEM=TC "
                            "SBOXID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"PACKGATE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SBOXID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SET_PACKING_WEIGHT" )     
    {
        TMsg.Format((char *)"SET_PACKING_WEIGHT HDR=(LOTmgr,%s,SET_PACKING_WEIGHT) EQPID=%s LBOXID=%s TYPE=%s WEIGHT=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LBOXID"),
                TFilter.GetArg((char *)"TYPE"),
                TFilter.GetArg((char *)"WEIGHT"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if ( TCmd == "PMSSPLIT" )    
    {
        // PMSSPLIT HDR=(,,) [LOTID=] [BASE_OBJECT_ID=] OPERID= CHILD_BASE_OBJECT_ID= [SPLITINFO=] [SPLITTYPE=] [REQ_SYSTEM=] 
        //if( CheckPara(TFilter, "LOTID"                    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
    //        if( CheckPara(TFilter, "CHILD_BASE_OBJECT_ID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"PMSSPLIT HDR=(LOTmgr,%s,PMSSPLIT) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                                "CHILD_BASE_OBJECT_ID=%s ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    (TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"CHILD_BASE_OBJECT_ID") );

        AddMsg( TFilter, TMsg, "LOTID"                   , "LOTID"                              );
        AddMsg( TFilter, TMsg, "BASE_OBJECT_ID"          , "BASE_OBJECT_ID"                     );
        AddMsg( TFilter, TMsg, "SPLITINFO"               , "SPLITINFO"                          );
        AddMsg( TFilter, TMsg, "SPLITTYPE"               , "SPLITTYPE"                          );
        AddMsg( TFilter, TMsg, "CHILD_BASE_OBJECT_ID"    , "CHILD_BASE_OBJECT_ID"               );

        TMesTarget = "SIMAX";

    }
    else if ( TCmd == "PMSMERGE" )    
    {
        //PMSMERGE HDR=(,,) [LOTID=] [BASE_OBJECT_ID=] OPERID= [SUBLOTID=] [SUB_BASE_OBJECT_ID=] [REQ_SYSTEM=] 
        if( CheckPara(TFilter, "OPERID"                    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }


        TMsg.Format((char *)"PMSMERGE HDR=(LOTmgr,%s,PMSSPLIT) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"EQPID"),
                    (TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        AddMsg( TFilter, TMsg, "LOTID"                  , "LOTID"                     );
        AddMsg( TFilter, TMsg, "BASE_OBJECT_ID"         , "BASE_OBJECT_ID"            );
        AddMsg( TFilter, TMsg, "SUBLOTID"               , "SUBLOTID"                  );
        AddMsg( TFilter, TMsg, "SUB_BASE_OBJECT_ID"     , "SUB_BASE_OBJECT_ID"        );

        TMesTarget = "SIMAX";

    }
    else if ( TCmd == "REEL_MERGE" )    
    {
        if( CheckPara(TFilter, "FIR_BAR_NO"                    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "SEC_BAR_NO"                    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "REQ_TYPE"                      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"REEL_MERGE HDR=(LOTmgr,%s,REEL_MERGE) FIR_BAR_NO=\"%s\" SEC_BAR_NO=\"%s\" EQPID=%s OPERID=%s REQ_TYPE=%s REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"FIR_BAR_NO"),
                    TFilter.GetArg((char *)"SEC_BAR_NO"),
                    TFilter.GetArg((char *)"EQPID"),
                    (TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"REQ_TYPE"));

        AddMsg( TFilter, TMsg, "NEW_SRLNO"                  , "NEW_SRLNO"             );

        TMesTarget = "SIMAX";
    }
    else if ( TCmd == "BADWAFERMNG" )    
    {
        if( CheckPara(TFilter, "WAFERID"                    , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "DACNT"                      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"BADWAFERMNG HDR=(LOTmgr,%s,BADWAFERMNG) WAFERID=%s OPERID=%s LINEID=%s EQPID=%s DACNT=%s REQ_SYSTEM=TC ",
                    m_TRvMosSubjectHDR.GetPrintString(),
                    TFilter.GetArg((char *)"WAFERID"),
                    ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                    TFilter.GetArg((char *)"LINE"),
                    TFilter.GetArg((char *)"EQPID"),
                    TFilter.GetArg((char *)"DACNT"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "RESERVEWORKCANCEL" ) 
    {
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->Write((char *)"[ERR] GetTrackingMsg, CheckPara() %s", TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"RESERVEWORKCANCEL HDR=(LOTmgr,%s,RESERVEWORKCANCEL) LINE=%s EQPID=%s OPERID=%s LOTID=%s MES_TARGET=WORKMAN REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                ( TFilter.GetArg((char *)"LINE")    == NULL ) ? ""     : TFilter.GetArg((char *)"LINE"),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "EQP_LOADER_EMPTY_REQ" )   
    {
        if( CheckPara(TFilter, "PORTID"          , TBuf) == 0 ) { m_pTLog->Write((char *)"[ERR] GetTrackingMsg, CheckPara() %s", TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EQP_LOADER_EMPTY_REQ HDR=(LOTmgr,%s,EQP_LOADER_EMPTY_REQ)  EQPID=%s OPERID=%s PORTID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PORTID"));

        AddMsg( TFilter, TMsg, "LOTID"                  , "LOTID"                     );
        AddMsg( TFilter, TMsg, "APLOTID"                , "APLOTID"                   );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "WORK_REQ_INFO" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WORK_REQ_INFO HDR=(LOTmgr,%s,WORK_REQ_INFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "GROUP_NAME"    , "GROUP_NAME"     );
        AddMsg( TFilter, TMsg, "WORK_TYPE"       , "WORK_TYPE"        );
		AddMsg( TFilter, TMsg, "REQTYPE"           , "REQTYPE"            ); 

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CHECK_INLINE_MATERIAL" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHECK_INLINE_MATERIAL HDR=(LOTmgr,%s,CHECK_INLINE_MATERIAL) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "WORKCHK" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"WORKCHK HDR=(LOTmgr,%s,WORKCHK) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s WAFERID=%s TYPE=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"), TFilter.GetArg((char *)"WAFERID"), TFilter.GetArg((char *)"TYPE") );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "GET_COMPONENTID" )
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"GET_COMPONENTID HDR=(LOTmgr,%s,GET_COMPONENTID) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s WAFERID=%s SLOTNUMBER=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"), TFilter.GetArg((char *)"WAFERID"), TFilter.GetArg((char *)"SLOTNUMBER") );

        TMesTarget = "SIMAX";

    }else if( TCmd == "DEVICE_CHG_RESRV_REQ_REP" )
    {
        if( CheckPara(TFilter, "EQPID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STATUS"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "AUTO_CHG_FLAG"  , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"DEVICE_CHG_RESRV_REQ_REP HDR=(LOTmgr,%s,DEVICE_CHG_RESRV_REQ_REP) STATUS=%s EQPID=%s BOARD_TYPE=%s TRAY_SPEC=%s AUTO_CHG_FLAG=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"STATUS"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"BOARD_TYPE"),
                TFilter.GetArg((char *)"TRAY_SPEC"),
                TFilter.GetArg((char *)"AUTO_CHG_FLAG") );

                AddMsg( TFilter, TMsg, "BOARD_CNT"  , "BOARD_CNT"   );

        TMesTarget = "WORKMAN";
    }

    else if( TCmd == "CARCREATE" ) 
    {
        //CARCREATE HDR=(WORKMAN,LH.BLAP24707392,CARCREATE) CARID=173778S LOTID=EFOUP EQPID=1BL006 OPERID=AUTO 
        //RETURNFABFLAG=Y CARTYPE=- SOURCESITE=XFB1 SENDLINEFLAG=XEDS  REQ_SYSTEM=TC

        TMsg.Format((char *)"CARCREATE HDR=(LOTmgr,%s,CARCREATE) EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        AddMsg( TFilter, TMsg, "CARID"                  , "CARID"                     );
        AddMsg( TFilter, TMsg, "LOTID"                  , "LOTID"                     );
        AddMsg( TFilter, TMsg, "RETURNFABFLAG"          , "RETURNFABFLAG"             );
        AddMsg( TFilter, TMsg, "CARTYPE"                , "CARTYPE"                   );
        AddMsg( TFilter, TMsg, "SOURCESITE"             , "SOURCESITE"                );
        AddMsg( TFilter, TMsg, "SENDLINEFLAG"           , "SENDLINEFLAG"              );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "CARRIER_CLEAN_REQ" )
    {
        //CARRIER_CLEAN_REQ HDR=(WORKMAN,LH.BLAP41944552,CARRIER_CLEAN_REQ) CARRIERID=173778S LOTID=EFOUP EQPID=1BL006 
        //OPERID=WORKMAN LINE=XAS1  REQ_SYSTEM=TC

        TMsg.Format((char *)"CARRIER_CLEAN_REQ HDR=(LOTmgr,%s,CARRIER_CLEAN_REQ)  EQPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        AddMsg( TFilter, TMsg, "LOTID"                  , "LOTID"                     );
        AddMsg( TFilter, TMsg, "CARRIERID"              , "CARRIERID"                 );
        AddMsg( TFilter, TMsg, "LINE"                   , "LINE"                      );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "AUTORECVREQ" ) 
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"AUTORECVREQ HDR=(LOTmgr,%s,AUTORECVREQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "SMT_WORK_INFO" ) // 2021.11.04 AJS : PCB INFO SECOND ADD
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SMT_WORK_INFO HDR=(LOTmgr,%s,SMT_WORK_INFO) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s PCBSIDE=%s",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"PCBSIDE"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "EQP_WORKOUT_REQ" ) 
    {
        if( CheckPara(TFilter, "LOTID"      , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"EQP_WORKOUT_REQ HDR=(LOTmgr,%s,EQP_WORKOUT_REQ) EQPID=%s OPERID=%s REQ_SYSTEM=TC "
                            "LOTID=%s ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LOTID") );

        AddMsg( TFilter, TMsg, "VISIONQTY"              , "VISIONQTY"                   );
        AddMsg( TFilter, TMsg, "CARRIERID"              , "CARRIERID"                   );
        AddMsg( TFilter, TMsg, "PORTID"                 , "PORTID"                      );
        AddMsg( TFilter, TMsg, "WORK_RESULT"            , "WORK_RESULT"                 );
        AddMsg( TFilter, TMsg, "FAIL_REASON"            , "FAIL_REASON"                 );
        AddMsg( TFilter, TMsg, "FAIL_ARGUMENT"          , "FAIL_ARGUMENT"               );
        AddMsg( TFilter, TMsg, "DUMPCOMPLT_YN"          , "DUMPCOMPLT_YN"               );
        AddMsg( TFilter, TMsg, "EQPTYPE"                , "EQPTYPE"                     );
        AddMsg( TFilter, TMsg, "ETRAY_EXNO"             , "ETRAY_EXNO"                  );

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "CHK_MERGE_RESRV_INFO" ) 
    {
        if( CheckPara(TFilter, "EQPID"              , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "OLD_RESRV_SEQ_ID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "NEW_RESRV_SEQ_ID"   , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHK_MERGE_RESRV_INFO HDR=(LOTmgr,%s,CHK_MERGE_RESRV_INFO) EQPID=%s OLD_RESRV_SEQ_ID=%s NEW_RESRV_SEQ_ID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"OLD_RESRV_SEQ_ID"),
                TFilter.GetArg((char *)"NEW_RESRV_SEQ_ID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CCS_RESERVED_CONFIRM" ) 
    {
        if( CheckPara(TFilter, "EQPID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "COMB"           , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "STEPSEQ"        , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CCS_RESERVED_CONFIRM HDR=(LOTmgr,%s,CCS_RESERVED_CONFIRM) EQPID=%s COMB=%s STEPSEQ=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"COMB"),
                TFilter.GetArg((char *)"STEPSEQ"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CHG_SLOT_INFO" ) 
    {

        // CHG_SLOT_INFO HDR=(TRACKING,LH.OYTCAP04G03_W07,47972871_001) TRANSACTIONID=47972871_001 OPERID=AUTO LOTID=A2WT722 TYPE=FRAME FRAME_LIST=(S8443429233604079:1,S8443429233604062:2,S8443429233604137:3,S8443429233604107:4,S8443429233604033:5,S8443429233604142:6,S844342923360422A:7,S8443429233604192:8,S8443429233604122:9,S8443429233604246:10,S8443429233604229:11,S8443429233604076:12,S8443429233604077:13,S8443429233604249:14,S8443429233604301:15,S8443429233604233:16) REQ_SYSTEM=TC

        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "TYPE"           , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "FRAME_LIST"     , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHG_SLOT_INFO HDR=(LOTmgr,%s,CHG_SLOT_INFO) LOTID=%s TYPE=%s OPERID=%s FRAME_LIST=(%s) REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"TYPE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"FRAME_LIST"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MVPPKGMAP" ) 
    {
        // MVPPKGMAP HDR=(TRACKING,LH.TBLG88_REALEQPTEST,45188780_001) TRANSACTIONID=45188780_001 LOTID=GFO09700B5 OPERID=AUTO REQ_SYSTEM=TC

        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MVPPKGMAP HDR=(LOTmgr,%s,MVPPKGMAP) LOTID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "MNG_RACK_SLOT" ) 
    {
        if( CheckPara(TFilter, "EQPID"            , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "RACK_ID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "REQ_TYPE"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"MNG_RACK_SLOT HDR=(LOTmgr,%s,MNG_RACK_SLOT) EQP_ID=%s RACK_ID=%s REQ_TYPE=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"RACK_ID"),
                TFilter.GetArg((char *)"REQ_TYPE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

                AddMsg( TFilter, TMsg, "LOT_ID"                 , "LOT_ID"                          );
                AddMsg( TFilter, TMsg, "NEW_SLOT_INFO"          , "NEW_SLOT_INFO"       , "(", ")"  );
                AddMsg( TFilter, TMsg, "REPAIR_BOARD_INFO"      , "REPAIR_BOARD_INFO"   , "(", ")"  );
                AddMsg( TFilter, TMsg, "LOT_LIST"               , "LOT_LIST"            , "(", ")"  );  

        TMesTarget = "WORKMAN";
    }
    else if( TCmd == "LOTRECIPEINFO" ) 
    {
        //LOTRECIPEINFO HDR=(TRACKING,LH.TBLG36_JEONGEUNLEE_DEV,82137862_002) TRANSACTIONID=82137862_002 LOTID=GJLVK78X EQPID=MD305 REQ_TYPE=REQUEST OPERID=AUTO REQ_SYSTEM=TC
        //LOTRECIPEINFO_REP HDR=(TRACKING,LH.TBLG36_JEONGEUNLEE_DEV,82137862_002) STATUS=PASS RECIPE_ID=- REQ_SYSTEM=TC TRANSACTIONID=82137862_002

        if( CheckPara(TFilter, "EQPID"            , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "LOTID"          , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }
        if( CheckPara(TFilter, "REQ_TYPE"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"LOTRECIPEINFO HDR=(LOTmgr,%s,LOTRECIPEINFO) EQPID=%s LOTID=%s REQ_TYPE=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"REQ_TYPE"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }else if( TCmd == "GET_PCBID" ) 
    {
        //GET_PCBID HDR=(IMSmgr,LH.MLCC8326,GET_PCBID) LOTID=SL1026804 OPERID=06375071 EQPID=1ILD01 EQPTYPE=MLCC PORTID=1 TYPE=NEW REQ_SYSTEM=TC
        //GET_PCBID HDR=(IMSmgr,LH.MLCC21143,GET_PCBID) LOTID=5B02202624V.03 OPERID=AUTO EQPID=5ILD01 EQPTYPE=MLCC PORTID=1 TYPE=CHECK PCBID=S85705S51D0110011 REQ_SYSTEM=TC

        TMsg.Format((char *)"GET_PCBID HDR=(LOTmgr,%s,GET_PCBID) LOTID=%s OPERID=%s EQPID=%s EQPTYPE=%s PORTID=%s TYPE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"EQPTYPE"),
                TFilter.GetArg((char *)"PORTID"),
                TFilter.GetArg((char *)"TYPE"));

        AddMsg( TFilter, TMsg, "PCBID"     , "PCBID"                      );
        AddMsg( TFilter, TMsg, "TRANSACTIONID"      , "TRANSACTIONID"       );

        TMesTarget = "SIMAX";
    }else if ( TCmd == "ASSYCCS") 
    {
        //ASSYCCS HDR=(TRACKING,LH.AA,AA) CMD=CHECKIMPORT CMDVER=1.0 OPERID= PARAMLIST=(SUBCMD=CHKIMPORT1,MATLOTLIST=M0609-851:0202-001899:I02K) DELIMITER=YES
        TMsg.Format((char *)"ASSYCCS HDR=(LOTmgr,%s,ASSYCCS) CMD=%s CMDVER=%s OPERID=%s PARAMLIST=(%s) REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"CMD"),
                TFilter.GetArg((char *)"CMDVER"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PARAMLIST"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "WAFERMAP" ) 
    {
        TMsg.Format((char *)"WAFERMAP HDR=(LOTmgr,%s,WAFERMAP) LOTID=%s OPERID=%s EQPID=%s WAFERID=%s RINGID=%s REASON=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"WAFERID"),
                TFilter.GetArg((char *)"RINGID"),
                TFilter.GetArg((char *)"REASON"));

        AddMsg( TFilter, TMsg, "TRANSACTIONID"      , "TRANSACTIONID"       );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "GET_PACKINGMASTER" )//2024.1.15 AJS : Module Packing Add
    {
//        if( CheckPara(TFilter, "PRODSERIAL"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"GET_PACKINGMASTER HDR=(LOTmgr,%s,GET_PACKINGMASTER) LOTID=%s OPERID=%s PRODSERIAL=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PRODSERIAL"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_SBOXLABEL" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "SBOXID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_SBOXLABEL HDR=(LOTmgr,%s,SSD_SBOXLABEL) SBOXID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"SBOXID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_SBOXMAKE" )//2024.1.15 AJS : Module Packing Add
    {
        TMsg.Format((char *)"SSD_SBOXMAKE HDR=(LOTmgr,%s,SSD_SBOXMAKE) OPERID=%s PACKGATE=%s SINGLEMAKE=%s LOTID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PACKGATE"), TFilter.GetArg((char *)"SINGLEMAKE"), TFilter.GetArg((char *)"LOTID"));
        AddMsg( TFilter, TMsg, "SERIAL"               , "SERIAL",             "(",    ")"     );
        AddMsg( TFilter, TMsg, "SUBCMDS"              , "SUBCMDS"                             );
        AddMsg( TFilter, TMsg, "WEIGHT"               , "WEIGHT"                              );
        AddMsg( TFilter, TMsg, "SEQ"                  , "SEQ"                                 );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_LBOXLABEL" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LBOXID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_LBOXLABEL HDR=(LOTmgr,%s,SSD_LBOXLABEL) LBOXID=%s OPERID=%s PACKGATE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LBOXID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PACKGATE"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_LBOXMAKE" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "SBOXLIST"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_LBOXMAKE HDR=(LOTmgr,%s,SSD_LBOXMAKE) LOTID=%s SBOXCNT=%s OPERID=%s SBOXLIST=(%s) PACKGATE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"SBOXCNT"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SBOXLIST"), 
                TFilter.GetArg((char *)"PACKGATE"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SLIPDELETE" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "SLIPID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SLIPDELETE HDR=(LOTmgr,%s,SSD_LBOXMAKE) SLIPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"SLIPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_SBOXDELETE" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_SBOXDELETE HDR=(LOTmgr,%s,SSD_SBOXDELETE) LOTID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_LBOXDELETE" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LBOXID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_LBOXDELETE HDR=(LOTmgr,%s,SSD_LBOXDELETE) LBOXID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LBOXID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_CHKTESTRESULTTRAY" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_CHKTESTRESULTTRAY HDR=(LOTmgr,%s,SSD_CHKTESTRESULTTRAY) LOTID=%s OPERID=%s SERIAL_CNT=%s %s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SERIAL_CNT"),
                TFilter.GetArg((char *)"SERIAL_INFO"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_CHKTESTRESULT" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SSD_CHKTESTRESULT HDR=(LOTmgr,%s,SSD_CHKTESTRESULT) LOTID=%s OPERID=%s SUBCMDS=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"SUBCMDS"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SLIPMAKE" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SLIPMAKE HDR=(LOTmgr,%s,SLIPMAKE) LOTID=%s OPERID=%s LBOXLIST=(%s) REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"LBOXLIST"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SLIPSEND" )//2024.1.15 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "SLIPID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"SLIPSEND HDR=(LOTmgr,%s,SLIPSEND) SLIPID=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"SLIPID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "CHKSPLIT" )//2024.5.13 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"CHKSPLIT HDR=(LOTmgr,%s,CHKSPLIT) LOTID=%s OPERID=%s TYPE=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"TYPE"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "IF_AUTOPACKING" )//2024.5.13 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "BOATID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"IF_AUTOPACKING HDR=(LOTmgr,%s,IF_AUTOPACKING) LOTID=%s OPERID=%s BOATID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"BOATID"));

        AddMsg( TFilter, TMsg, "PRODSERIAL"           , "PRODSERIAL"                             );
        AddMsg( TFilter, TMsg, "ACTIVITY"             , "ACTIVITY"                               );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "DENSITYLABELCCS" )//2024.5.13 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "PRODSERIAL"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"DENSITYLABELCCS HDR=(LOTmgr,%s,DENSITYLABELCCS) LOTID=%s OPERID=%s PRODSERIAL=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PRODSERIAL"));

        AddMsg( TFilter, TMsg, "DENSITY_LABEL_MATL_CODE"           , "DENSITY_LABEL_MATL_CODE"                             );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "DTBTLABELCCS" )//2024.5.13 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "PRODSERIAL"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"DTBTLABELCCS HDR=(LOTmgr,%s,DTBTLABELCCS) LOTID=%s OPERID=%s PRODSERIAL=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PRODSERIAL"));

        AddMsg( TFilter, TMsg, "DTBT_MATL_CODE"             , "DTBT_MATL_CODE"                             );
        AddMsg( TFilter, TMsg, "RIBBON_MATL_CODE"           , "RIBBON_MATL_CODE"                             );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TURNKEYBOX" )//2024.5.13 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TURNKEYBOX HDR=(LOTmgr,%s,TURNKEYBOX) LOTID=%s OPERID=%s BOATID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"BOATID"));

        AddMsg( TFilter, TMsg, "MATPART"             , "MATPART"                                 );
        AddMsg( TFilter, TMsg, "RIBBON_CODE"         , "RIBBON_CODE"                             );
        AddMsg( TFilter, TMsg, "PRODSRL"             , "PRODSRL"                                 );
        AddMsg( TFilter, TMsg, "DENS_CODE"           , "DENS_CODE"                               );
        AddMsg( TFilter, TMsg, "TURNKEY_CODE"        , "TURNKEY_CODE"                            );
        AddMsg( TFilter, TMsg, "TRAY_CODE"           , "TRAY_CODE"                               );
        AddMsg( TFilter, TMsg, "TRAY_TOP_COVER"      , "TRAY_TOP_COVER"                          );
        AddMsg( TFilter, TMsg, "WARRANTY_MATL_CODE"  , "WARRANTY_MATL_CODE"                      );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TURNKEYBOXCCS" )//2024.5.13 AJS : Module Packing Add
    {
        if( CheckPara(TFilter, "PRODSERIAL"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.GetPrintString(), TBuf.GetPrintString()); TErrMsg = TBuf; return FALSE; }

        TMsg.Format((char *)"TURNKEYBOXCCS HDR=(LOTmgr,%s,TURNKEYBOXCCS) PRODSERIAL=%s OPERID=%s REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"PRODSERIAL"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"));

        AddMsg( TFilter, TMsg, "DENSITY_MATL_CODE"             , "DENSITY_MATL_CODE"                                 );
        AddMsg( TFilter, TMsg, "TURNKEYBOX_MATL_CODE"          , "TURNKEYBOX_MATL_CODE"                              );

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "GET_CERTIFICATE" )//2024.6.03 AJS :
    {
        TMsg.Format((char *)"GET_CERTIFICATE HDR=(LOTmgr,%s,GET_CERTIFICATE) LOTID=%s OPERID=%s PRODID=%s SSD_SERIAL=%s CSR=\"%s\" REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"PRODID"),
                TFilter.GetArg((char *)"SSD_SERIAL"),
                TFilter.GetArg((char *)"CSR"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "SSD_SETTESTRESULT" )//2024.6.03 AJS :
    {
        TMsg.Format((char *)"SSD_SETTESTRESULT HDR=(LOTmgr,%s,SSD_SETTESTRESULT) LOTID=%s OPERID=%s ATTR=(%s) REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"ATTR"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "PACKING_MAT_ISSUE" )//2024.6.03 AJS :
    {
        TMsg.Format((char *)"PACKING_MAT_ISSUE HDR=(LOTmgr,%s,SSD_SETTESTRESULT) LOTID=%s OPERID=%s EQPID=%s MAT_LIST=(%s) REQ_SYSTEM=TC ",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                ( TFilter.GetArg((char *)"OPERID")  == NULL ) ? "AUTO" : TFilter.GetArg((char *)"OPERID"),
                TFilter.GetArg((char *)"EQPID"),
                TFilter.GetArg((char *)"MAT_LIST"));

        TMesTarget = "SIMAX";
    }
    else if( TCmd == "TESTRESLTCREATE" )//2024.12 CGH : MODULE PCBOX_END ���� TESTRESLTCREATE �߰�
    {
    	if( CheckPara(TFilter, "LOTID"         , TBuf) == 0 ) { m_pTLog->WriteTC(eLOG_TYPE_ERROR, eLOG_IFS_NONE, eTAB1, "GetTrackingMsg, CheckPara() CMD=%s : PARA_NAME=%s", TCmd.String(), TBuf.String()); TErrMsg = TBuf; return FALSE; }
        
        TMsg.Format((char *)"TESTRESLTCREATE HDR=(LOTmgr,%s,TESTRESLTCREATE) LOTID=%s PROD_SRL_NO=%s TEST_RESULT_SEG=%s CHKCNT=%s REQ_SYSTEM=TC",
                m_TRvMosSubjectHDR.GetPrintString(),
                TFilter.GetArg((char *)"LOTID"),
                TFilter.GetArg((char *)"PROD_SRL_NO"),
                TFilter.GetArg((char *)"TEST_RESULT_SEG"),
                TFilter.GetArg((char *)"CHKCNT"));

        TMesTarget = "SIMAX";
    }
    else
    {
        return FALSE;
    }

    if ( TMesTargetData == "SIMAX" || TMesTargetData == "WORKMAN" || TMesTargetData == "SIMAX_OI" )
    {
        TMesTarget = TMesTargetData;
    }

    return TRUE;
}

bool CWorkflow::TC_CMD(CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg)
{
    int     nRet = FALSE;


    m_TReplyMsg = "";

    if(      strncmp(TMsg.GetPrintString(), "CMD=S1F13 "                    , strlen("CMD=S1F13 ")                  ) == 0 ) nRet = this->TC_S1F14                  (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=S1F14 "                    , strlen("CMD=S1F14 ")                  ) == 0 ) nRet = this->TC_S1F14                  (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=S5F1 "                     , strlen("CMD=S5F1 ")                   ) == 0 ) nRet = this->TC_S5F1                   (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=S14F1 "                    , strlen("CMD=S14F1 ")                  ) == 0 ) nRet = this->TC_S14F1                  (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);

    else if( strncmp(TMsg.GetPrintString(), "CMD=COMM_STATUS "              , strlen("CMD=COMM_STATUS ")            ) == 0 ) nRet = this->TC_CommStatus             (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=TOOLDATA "                 , strlen("CMD=TOOLDATA ")               ) == 0 ) nRet = this->TC_ToolData               (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=TOOLALARM "                , strlen("CMD=TOOLALARM ")              ) == 0 ) nRet = this->TC_ToolAlarm              (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);


    else if( strncmp(TMsg.GetPrintString(), "CMD=EQP_RECIPE_UPLOAD "        , strlen("CMD=EQP_RECIPE_UPLOAD ")      ) == 0 ) nRet = this->RMM_EqpRecipeUpload       (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=RMS_RECIPE_UPLOAD_REP "    , strlen("CMD=RMS_RECIPE_UPLOAD_REP ")  ) == 0 ) nRet = this->RMM_RmsRecipeUploadRep    (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=EQP_RECIPE_DOWNLOAD "      , strlen("CMD=EQP_RECIPE_DOWNLOAD ")    ) == 0 ) nRet = this->RMM_EqpRecipeDownload     (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=RMS_RECIPE_DOWNLOAD_REP "  , strlen("CMD=RMS_RECIPE_DOWNLOAD_REP ")) == 0 ) nRet = this->RMM_RmsRecipeDownloadRep  (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=FDC_RECIPE_UPLOAD "        , strlen("CMD=FDC_RECIPE_UPLOAD ")      ) == 0 ) nRet = this->RMM_FdcRecipeUpload       (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    //2020.04.16 AJS : ��� File Upload��� �߰�
    else if( strncmp(TMsg.GetPrintString(), "CMD=EQP_FILE_UPLOAD "         , strlen("CMD=EQP_FILE_UPLOAD ")         ) == 0 ) nRet = this->RMM_EqpFileUpload         (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);

    else if( strncmp(TMsg.GetPrintString(), "CMD=WAFER_MAP_SETUP "          , strlen("CMD=WAFER_MAP_SETUP " )       ) == 0 ) nRet = this->WaferMap_Download         (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);
    else if( strncmp(TMsg.GetPrintString(), "CMD=WAFER_MAP_UPLOAD "         , strlen("CMD=WAFER_MAP_UPLOAD " )      ) == 0 ) nRet = this->WaferMap_Upload           (TMsg, TBinMsg, TPPBodyMsg, TDmsMsg, TReplyMsg);

    else                                                                                                             TReplyMsg = "RESULT=FAIL MSG=\"UNKNOWN CMD\"";

    // StartSubMessage, EndSubMessage, MakeMessage���� ȣ���ؼ� Message�� �����ߴٸ� ������ �޼����� �����Ѵ�
    if( m_TReplyMsg.Length() > 0 ) TReplyMsg = m_TReplyMsg;

    return nRet;
}
