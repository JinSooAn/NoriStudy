#ifndef __WORKFLOW__
#define __WORKFLOW__

#include <tibrv/tibrvcpp.h>
#include "HSMSLib.hpp"
#include "COracle.hpp"
#include "CRemoteCmdMsg.hpp"
#include "CObjAttrMsg.hpp"
#include "CEcidMsg.hpp"

class CKafka;

#ifndef __MAP_STRUCT__
#define __MAP_STRUCT__

typedef struct
{
    CString     TFrameId;
    int         nTotalCol;
    int         nTotalRow;
    int         nBlockCol;
    int         nBlockRow;
    int         nTotalQty;
    int         nGoodQty;
    int         nRejectQty;
    CString     TMapData;
    CString     TMapSeqData;
    CString     TMCPSeq;
    CString     TWaferBin;
    CString     TChipIDInfo;
    CString     TPkgID;
    CString     TLocationXY;
    CString     TLastStep;
    CString     TDeviceID;
    CString     TVender;
    CString     TEngCode;
    CString     TRevision;
} MAPINFO;

#endif

class CWorkflow
{
protected:

    CRvMain*    m_pTRv;
    CRvMain*    m_pTEqpRv;
    CRvMain*    m_pTMOS_Rv;
    CRvMain*    m_pTFDC_Rv;
    CRvMain*    m_pTRMM_Rv;
    CKafka*     m_pDRT;          // DRT 전용 Kafka 송신 래퍼

    CSocket*    m_pTSocket;

    CLogFile*   m_pTLog;
    CIniFile*   m_pTEnv;

    CLogFile    m_TQALog;

    COracle     m_TOra;

    CString     m_TRvSubject;
    CString     m_TEqpRvSubject;
    CString     m_TRvMosSubject;
    CString     m_TRvMosSubjectHDR;

    CString     m_TReplyMsg;
    
    CString     m_TBinData;
    CString     m_TBodyData;
    CString     m_TDmsData;

    CString     m_TMsgSeq;

    CString     m_TTmpPath;

    CString     GetBinData                  ();
    CString     GetBodyData                 ();
    CString     GetDmsData                  ();
    
    bool        CheckPara                   (CFilter &TFilter, CString TParaName, CString &TErrorMsg);
    bool        AddMsg                      (CFilter &TMsgFilter, CString &TMsg, CString TDataName, CString TMsgName, CString TLeftBrace = "", CString TRightBrace = "");
    CString     GetEqpSubject               (CString TEqpID);
    CString     GetEqpSubject_Sub           (CString TEqpID);
    CString     GetRPTInfo                  (CFilter &TFilter, CString TItem1, CString TItem2 = "");
    CString     GetFDCSubject               ( );

    // Simax/WorkMan ��ſ� ���� �Լ���
    virtual CString GetArgfromMES           (CFilter &TFilter, CString TItemName, CString TItemName2 = "");
    virtual bool GetTrackingMsg             (CString TCmd, CString TData, CString &TMsg, CString &TErrMsg, CString &TMesTarget);
    virtual bool MES_Send                   (CString &TMsg, CString &TReplyMsg, int nWaitReply = TRUE);
    virtual bool WORKMAN_Send               (CString &TMsg, CString &TReplyMsg, int nWaitReply = TRUE);
    virtual bool MES_Comm                   (CString TCmd, CString TData, CString &TRetMsg, int nWaitReply = TRUE);
    virtual bool MES_Comm                   (CString TCmd, CString TData, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = TRUE);
    virtual bool DRT_Comm                   (CString TCmd, CString TData, CString &TRetMsg, int nWaitReply = TRUE); // DRT 전용 Kafka 메시지 전송
    virtual bool MES_Comm_OiSocket          (CString TTarIP, CString TTarPort, CString &TMsg, CString &TReplyMsg, int *npTimeout = NULL);

    // EES,FDC ��ſ� ���� �Լ���
    virtual bool FDC_Send                   (CString TSubject, CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg);
    virtual bool EES_Send                   (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg);
    virtual bool SPC_Send                   (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg);
    virtual bool EES_Send_Wait              (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, CString &TEqpID, int nWaitReply = FALSE);
    virtual bool ANOMALY_Send               (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = FALSE);

    // ���� ��ſ� ���� �Լ���
    virtual bool EQP_Send                   (CString TSubject, CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = FALSE);

    // INKLESS ��ſ� ���� �Լ���
    virtual bool Inkless_Comm               (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = FALSE);
    virtual bool Etc_Comm                   (CString &TMsg, CString TKind,    CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = TRUE); // 2020-05-28

    //2023.09.19 LKM : MCTS RV ó�� ��� �߰�
    virtual bool MCTS_Comm                  (CString TCmd, CString TData, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = TRUE);

    virtual bool RV_SendLocal               (CString TTargetSubject, CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TRetMsg, int nWaitReply = FALSE);

    // Utility �Լ���
    virtual bool StartSubMessage            (char *pszName);
    virtual bool EndSubMessage              ();
    virtual bool MakeMessage                (char *pszName, char *pszValue, int nLen = 0);

public:
    CWorkflow(CIniFile *pTEnv, CLogFile *pTLog, TibrvNetTransport *pTransport);
    virtual ~CWorkflow();

    // ���񿡼� ���۵� �޼���. RECIPE �Ǵ� TRACKING CORE�� �������ؾ� �Ѵ�
    virtual bool TC_S1F14                   (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool TC_S5F1                    (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool TC_S14F1                   (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool TC_CommStatus              (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool TC_ToolData                (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, CString TSensorValue = "");
    virtual bool TC_ToolRecipe              (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, CString TSensorValue = "");
    virtual bool TC_ToolEvent               (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, CString TSensorValue = "", int nSendFDC = FALSE, int nSendEES = FALSE);
    virtual bool TC_ToolAlarm               (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg, int nSendFDC = FALSE, int nSendEES = FALSE);

    virtual bool TC_CMD                     (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);

    // RMM ���� �޼���
    virtual bool RMM_EqpRecipeUpload        (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool RMM_RmsRecipeUploadRep     (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool RMM_EqpRecipeDownload      (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool RMM_RmsRecipeDownloadRep   (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool RMM_FdcRecipeUpload        (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    //2020.04.16 AJS : ��� File Upload��� �߰�
    virtual bool RMM_EqpFileUpload          (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool RMM_GoldenRecipeUpload     (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool RMM_GoldenRecipeDownload   (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);

    // ǥ�� SECS/GEM �޼���
    virtual bool SECS_RemoteCommand         (CString TRCmdType, CString &TMsg, CString &TRetMsg, int nWaitReply = 0);
    virtual bool SECS_CarrierID_Verification(CString &TMsg, CString &TRetMsg);
    virtual bool SECS_SlotMap_Verification  (CString &TMsg, CString &TRetMsg);
    virtual bool SECS_ProcessJob_Create     (CString &TMsg, CString &TRetMsg);
    virtual bool SECS_ControlJob_Create     (CString &TMsg, CString &TRetMsg);

    virtual bool Get_Mats_Current           (CString &TMsg, CString TEqpID, CString &TRetVal, CString TMatType);
    virtual bool Get_Mats_RemainTime        (CString &TMsg, CString TMatLotID, CString &TRetVal);
    virtual bool Get_PCBList                (CString &TMsg, CString &TLotID, CString &TRetVal);
    virtual bool Get_SetLotInfo             (CString &TMsg, CString &TRetVal);

    virtual bool WaferMap_Download          (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual bool WaferMap_Upload            (CString &TMsg, CString &TBinMsg, CString &TPPBodyMsg, CString &TDmsMsg, CString &TReplyMsg);
    virtual void Rotaion_FrameMap           (CString T2DOrigin, MAPINFO *pMap);
    virtual void Rotaion_DeviceID           (CString T2DOrigin, int nCols, int nRows, CString &TDeviceID); // 2019-05-23
    virtual void Rotaion_ChipInfo           (CString T2DOrigin, int nCols, int nRows, CString &TChipIdInfo, CString &TEqpChipIdInfo, CString &TCellStatus); 
    virtual int  GetMapXPosRotation         (int nXPos, int nMax_X, CString T2DOrigin);
    virtual int  GetMapYPosRotation         (int nYPos, int nMax_Y, CString T2DOrigin);
    
    // ����� �޼��� ����
    virtual bool EQP_DispMsg                (CString TSubject, CString TMsg);

    virtual bool SetMsgSeq                  (CString TMsgSeq);

    // DB�� TC_EQP_STATUS ���̺� I/F
    virtual bool Get_DB_TC_EQP_STATUS       (CString TMsg, CString &TRetVal);
    virtual bool Set_DB_TC_EQP_STATUS       (CString TMsg, CString &TRetVal);

    // PMS Parsing
    virtual bool MOS_WaferPmsMap            (CString &TMsg, CString &TRetVal);
    virtual bool Wafer_PMS_Parsing          (CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal);
    virtual bool Boat_PMS_Parsing           (CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal);
    virtual bool Reel_PMS_Parsing           (CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal);
    //virtual bool HeatSlug_PMS_Parsing       (CString cszMsg, CString cszFile, CString &TMapBaseData, CString &TMapDetailData, CString &TRetVal);

};

#endif
