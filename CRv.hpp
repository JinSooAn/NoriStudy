#ifndef __RV_CLASS_HPP__
#define __RV_CLASS_HPP__

#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>

#include <tibrv/tibrvcpp.h>
#include <tibrv/cmcpp.h>
#include <tibrv/ftcpp.h>

//#define FIELD_NAME        "DATA"
//#define FIELD_NAME_BINARY "BDATA"

//#define RVCASTMODE        "BROAD"
//#define RVDAEMON_DEFAULT  "7520"

#include "../include/ipcConf.hpp"
#include "../include/preDefine.hpp"
#include "../include/rvdConfig.hpp"
#include "../include/HSMSLib.hpp"

class CRvSender
{
private:
    CLogFile *mainLog;

    TibrvNetTransport *m_pTransport;

public:
    CRvSender                                       ( CLogFile *pLog, TibrvNetTransport *pTransport);
    ~CRvSender                                      ( );

    //int                   SendRvMsg               ( CString cszToSubject, CString cszSendMsg, RV_SEND_TYPE ervSendType = MESSAGE, CString cszSendMsgBinary = "" );
    int                     SendRvMsg               ( CString cszToSubject, CString cszSendMsg, int ervSendType = MESSAGE, CString cszSendMsgBinary = "", CString cszSendMsgPPBODY = "", CString cszSendMsgDMS = "" );
};

//===============================================================

class CRvListener : public TibrvMsgCallback
{
private:

    BOOL                    m_rvRead;
    RV_BLOCK                m_BlockMode;

    CString                 m_rvReplySubject;
    CString                 m_rvmessage;
    CString                 m_rvmessageBinary;
    CString                 m_rvmessagePPBODY;
    CString                 m_rvmessageDMS;

    //RV_SEND_TYPE  m_rvReadType;
    int                     m_rvReadType;

    TibrvListener           *m_listener;

    CLogFile                *mainLog;

    TibrvNetTransport       *m_pTransport;

public:

    CRvListener                                     ( CLogFile *pLog, RV_BLOCK BlockMode, TibrvNetTransport *pTransport );
    ~CRvListener                                    ( );

    void                    onMsg                   ( TibrvListener* listener, TibrvMsg& msg ); 

    void                    Clear                   ( );
    int                     Create_Listner          ( CString rv_subject );
    //int                   GetRvMessage            ( CString &cszRvMsg,  RV_SEND_TYPE &eRvReadType, CString &cszRvMsgBin );
    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, CString &cszRvMsgBin, int nTimeout = 0 );

    //2007-12-07 YSJ �߰�
    //  EES I/F �� PPBODY, DMS �ʵ带 ó���ؾ� �Ǽ�...
    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, int nTimeout = 0 );
    CString                 GetBinBinary            ( );
    CString                 GetBinPPBODY            ( );  // PPBODY ����
    CString                 GetBinDMS               ( );     // DMS ����
    CString                 GetReplySubject         ( );
};

//CMQ
class CRvCMQListener : public TibrvCmMsgCallback
{
private:

    BOOL                    m_rvRead;
    RV_BLOCK                m_BlockMode;

    CString                 m_rvReplySubject;
    CString                 m_rvmessage;
    CString                 m_rvmessageBinary;
    CString                 m_rvmessagePPBODY;
    CString                 m_rvmessageDMS;

    //RV_SEND_TYPE  m_rvReadType;
    int                     m_rvReadType;

    CLogFile                *mainLog;

    TibrvNetTransport       *m_pTransport;

    TibrvCmQueueTransport   *m_pCMQTransport;

    TibrvQueue              *m_pQueue;
    TibrvCmListener         *m_pCMListener;

public:

    CRvCMQListener                                  ( CLogFile *pLog, RV_BLOCK BlockMode, TibrvNetTransport *pTransport );
    ~CRvCMQListener                                 ( );

    void                    onCmMsg                 ( TibrvCmListener* listener, TibrvMsg& msg ); 

    void                    Clear                   ( );
    int                     Create_Listner          ( CString rv_subject, CString cszDQIdentifyID );
    //int                   GetRvMessage            ( CString &cszRvMsg,  RV_SEND_TYPE &eRvReadType, CString &cszRvMsgBin );
    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, CString &cszRvMsgBin, int nTimeout = 0 );

    //2007-12-07 YSJ �߰�
    //  EES I/F �� PPBODY, DMS �ʵ带 ó���ؾ� �Ǽ�...
    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, int nTimeout = 0 );
    CString                 GetBinBinary            ( );
    CString                 GetBinPPBODY            ( );  // PPBODY ����
    CString                 GetBinDMS               ( );     // DMS ����
    CString                 GetReplySubject         ( );
};

// FT RV
class CRvFTListener : public TibrvMsgCallback, public TibrvFtMemberCallback
{
private:

    BOOL                    m_rvRead;
    RV_BLOCK                m_BlockMode;

    CString                 m_rvMySubject;
    CString                 m_rvReplySubject;
    CString                 m_rvmessage;
    CString                 m_rvmessageBinary;
    CString                 m_rvmessagePPBODY;
    CString                 m_rvmessageDMS;

    //RV_SEND_TYPE  m_rvReadType;
    int                     m_rvReadType;

    TibrvListener           *m_listener;

    CLogFile                *mainLog;

    TibrvNetTransport       *m_pTransport;

    TibrvFtMember           *m_pFTMember;

    bool                    active;

public:

    CRvFTListener                                   ( CLogFile *pLog, RV_BLOCK BlockMode, TibrvNetTransport *pTransport );
    ~CRvFTListener                                  ( );

    void                    onMsg                   ( TibrvListener* listener, TibrvMsg& msg ); 

    void                    Clear                   ( );

    void                    onFtAction              ( TibrvFtMember * ftMember,
                                                      const char*     groupName,
                                                      tibrvftAction   action );
    int                     Create_Listner          ( );
    int                     Destroy_Listner         ( );
    int                     Create_FTMember         ( TibrvNetTransport *pFTTransport, CString cszGroupName, long nWeight, long nActiveGoalNum, 
                                                      double fHBInterval, double fPrepareInterval, double fActivateInterval );

    void                    Set_MySubject           ( CString cszRvSubject );

    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, CString &cszRvMsgBin, int nTimeout = 0 );

    //2007-12-07 YSJ �߰�
    //  EES I/F �� PPBODY, DMS �ʵ带 ó���ؾ� �Ǽ�...
    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, int nTimeout = 0 );
    CString                 GetBinBinary            ( );
    CString                 GetBinPPBODY            ( );  // PPBODY ����
    CString                 GetBinDMS               ( );     // DMS ����
    CString                 GetReplySubject         ( );
};


//===============================================================

class CRvMain
{
private:
    TibrvNetTransport       m_Transport;
    TibrvNetTransport       m_FTTransport;

    RV_SENDER               m_SenderFlag;
    RV_LISTEN               m_ListenFlag;
    RV_BLOCK                m_BlockMode;
    RV_CAST                 m_CastMode;

    CString                 m_cszService;
    CString                 m_cszNetwork;
    CString                 m_cszDaemon;

    CRvListener             *m_pListener;
    CRvCMQListener          *m_pCMQListener;     // 2016.01.11 kilbum.lee : DQ(Distributed Queue) ��� �߰�
    CRvFTListener           *m_pFTListener;      // 2016.03.02 kilbum.lee : FT(Fault-Tolerance)   ��� �߰�
    CRvSender               *m_pSender;

    CLogFile                *mainLog;

    // 2016.03.02 kilbum.lee : FT(Fault-Tolerance)   ��� �߰�
    CString                 m_cszFTGroupName;
    long                    m_lnFTWeight;
    long                    m_lnFTActiveGoalNum;
    double                  m_dbFTHBInterval;
    double                  m_dbFTPrepareInterval; 
    double                  m_dbFTActivateInterval;

    // 2017.05.19 kilbum.lee : DQ(Distributed Queue)   ���� ��� �߰�
    CString                 m_cszDQIdentifyID;

    int                     m_rvOpen                ( );
    int                     m_Create_Transport      ( CString cszDescription );
    int                     m_Create_FT_Transport   ( CString cszDescription );

public:
    CRvMain                                         ( CLogFile *pLog, RV_SENDER SenderFlag = RV_SENDER_ON, RV_LISTEN ListenFlag = RV_LISTENER_ON, RV_BLOCK BlockMode = RV_MSG_BLOCK, 
                                                      RV_CAST CastMode = RV_BROADCAST, CString cszService = (char *)NULL, CString cszNetwork = (char *)NULL, CString cszDaemon = (char *)NULL );
    ~CRvMain                                        ( );

    int                     Rv_Init                 ( CString cszDescription, CString cszRvSubject, TibrvNetTransport *pTransport = NULL );
    void                    Set_FT_RV_Info          ( CString cszGroupName, long nWeight, long nActiveGoalNum, 
                                                      double fHBInterval, double fPrepareInterval, double fActivateInterval  );
    void                    Set_DQ_RV_Info          ( CString cszIdentifyID );

    //int                   GetRvMessage            ( CString &cszRvMsg,  RV_SEND_TYPE &eRvReadType, CString &cszRvMsgBin );
    //int                   SendRvMsg               ( CString cszToSubject, CString cszSendMsg, RV_SEND_TYPE ervSendType = MESSAGE, CString cszSendMsgBinary = "" );

    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, CString &cszRvMsgBin, int nTimeout = 0 );

    //2007-12-07 YSJ �߰�, ����
    //  EES I/F �� PPBODY, DMS �ʵ带 ó���ؾ� �Ǽ�...
    int                     GetRvMessage            ( CString &cszRvMsg,  int &eRvReadType, int nTimeout = 0 );
    CString                 GetBinData              ( int nRvReadType );

    CString                 GetBinBinary            ( );
    CString                 GetBinPPBODY            ( );  // PPBODY ����
    CString                 GetBinDMS               ( );     // DMS ����
    CString                 GetReplySubject         ( );
    TibrvNetTransport*      GetTransPort            ( );

    int                     SendRvMsg               ( CString cszToSubject, CString cszSendMsg, int ervSendType = MESSAGE, CString cszSendMsgBinary = "", CString cszSendMsgPPBODY = "", CString cszSendMsgDMS = "" );
};

#endif
