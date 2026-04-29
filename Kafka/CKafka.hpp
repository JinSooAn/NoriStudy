#ifndef __KAFKA_CLASS_HPP__
#define __KAFKA_CLASS_HPP__

#include <iostream>
#include <string>
#include <vector>
#include <librdkafka/rdkafkacpp.h>

// CKafka: RV 기반 클래스를 Kafka 통신으로 전환한 래퍼 클래스
// - librdkafka C++ API를 사용하여 Kafka Producer/Consumer 기능을 제공합니다.
// - 기존 RV 메시지 스타일에서 Kafka 메시지 발송/수신으로 변경되었습니다.

class CKafka
{
private:
    std::string                 m_brokers;          // Kafka 브로커 주소
    std::string                 m_topic;            // Kafka 토픽 이름
    std::string                 m_groupId;          // Kafka consumer group id

    RdKafka::Producer          *m_producer;         // Kafka producer 객체
    RdKafka::KafkaConsumer     *m_consumer;         // Kafka consumer 객체
    RdKafka::Conf              *m_conf;             // consumer 전용 설정

    std::string                 m_lastPayload;      // 마지막으로 받은 메시지 저장

    class DeliveryReportCb : public RdKafka::DeliveryReportCb
    {
    public:
        void dr_cb(RdKafka::Message &message) override
        {
            // 메시지 전송 결과 콜백
            if (message.err()) {
                std::cerr << "Kafka delivery failed: " << RdKafka::err2str(message.err()) << std::endl;
            }
            else {
                std::cout << "Kafka delivered message to topic " << message.topic_name()
                          << " [" << message.partition() << "] at offset " << message.offset() << std::endl;
            }
        }
    };

public:
    CKafka();
    ~CKafka();

    // Kafka 연결 초기화
    // cszBrokers: bootstrap.servers 값
    // cszTopic: 사용 토픽
    // cszGroupID: consumer group id (수신 기능 사용 시 필요)
    bool Init(const std::string &cszBrokers,
              const std::string &cszTopic,
              const std::string &cszGroupID = "ckafka-group");

    // Kafka 메시지 전송
    // 주로 cszTopic에 cszSendMsg를 보냅니다.
    bool SendMessage(const std::string &cszTopic,
                     const std::string &cszSendMsg,
                     int ervSendType = 0,
                     const std::string &cszSendMsgBinary = "",
                     const std::string &cszSendMsgPPBODY = "",
                     const std::string &cszSendMsgDMS = "");

    // Kafka 메시지 수신
    // nTimeout은 밀리초 단위입니다.
    bool ReceiveMessage(std::string &cszMsg,
                        int &eRvReadType,
                        std::string &cszMsgBin,
                        int nTimeout = 1000);
    bool ReceiveMessage(std::string &cszMsg,
                        int &eRvReadType,
                        int nTimeout = 1000);

    // RV 호환용 호출자 메서드. Kafka 전용으로는 m_lastPayload를 반환합니다.
    std::string GetBinData(int nRvReadType) const;
    std::string GetBinBinary() const;
    std::string GetBinPPBODY() const;
    std::string GetBinDMS() const;
    std::string GetReplySubject() const;

    // 내부 Kafka 객체에 직접 접근해야 할 경우 사용
    RdKafka::Producer* GetProducer() const;
    RdKafka::KafkaConsumer* GetConsumer() const;
};

#endif // __KAFKA_CLASS_HPP__
