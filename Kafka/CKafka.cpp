#include "CKafka.hpp"
#include <iostream>

CKafka::CKafka()
    : m_producer(nullptr)
    , m_consumer(nullptr)
    , m_conf(nullptr)
{
}

CKafka::~CKafka()
{
    // consumer와 producer는 역순으로 삭제합니다.
    if (m_consumer) {
        m_consumer->unsubscribe();
        delete m_consumer;
        m_consumer = nullptr;
    }

    if (m_producer) {
        m_producer->flush(5000);
        delete m_producer;
        m_producer = nullptr;
    }

    delete m_conf;
}

bool CKafka::Init(const std::string &cszBrokers,
                  const std::string &cszTopic,
                  const std::string &cszGroupID)
{
    std::string errstr;

    // Kafka 브로커 주소 및 토픽을 저장
    m_brokers = cszBrokers;
    m_topic = cszTopic;
    m_groupId = cszGroupID;

    // 공통 설정 생성
    m_conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    if (!m_conf) {
        std::cerr << "Failed to create Kafka global configuration" << std::endl;
        return false;
    }

    if (m_conf->set("bootstrap.servers", m_brokers, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set bootstrap.servers: " << errstr << std::endl;
        return false;
    }

    if (!m_groupId.empty()) {
        if (m_conf->set("group.id", m_groupId, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set group.id: " << errstr << std::endl;
            return false;
        }

        if (m_conf->set("enable.auto.commit", "true", errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set enable.auto.commit: " << errstr << std::endl;
            return false;
        }

        if (m_conf->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set auto.offset.reset: " << errstr << std::endl;
            return false;
        }
    }

    // producer 생성
    RdKafka::Conf *producer_conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    if (!producer_conf) {
        std::cerr << "Failed to create Kafka producer configuration" << std::endl;
        return false;
    }

    if (producer_conf->set("bootstrap.servers", m_brokers, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set bootstrap.servers for producer: " << errstr << std::endl;
        delete producer_conf;
        return false;
    }

    static DeliveryReportCb dr_cb;
    if (producer_conf->set("dr_cb", &dr_cb, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set delivery callback: " << errstr << std::endl;
        delete producer_conf;
        return false;
    }

    m_producer = RdKafka::Producer::create(producer_conf, errstr);
    delete producer_conf;
    if (!m_producer) {
        std::cerr << "Failed to create Kafka producer: " << errstr << std::endl;
        return false;
    }

    // consumer는 그룹 ID가 제공된 경우에만 생성
    if (!m_groupId.empty()) {
        m_consumer = RdKafka::KafkaConsumer::create(m_conf, errstr);
        if (!m_consumer) {
            std::cerr << "Failed to create Kafka consumer: " << errstr << std::endl;
            return false;
        }

        std::vector<std::string> topics = {m_topic};
        RdKafka::ErrorCode err = m_consumer->subscribe(topics);
        if (err) {
            std::cerr << "Failed to subscribe to topic: " << RdKafka::err2str(err) << std::endl;
            return false;
        }
    }

    return true;
}

bool CKafka::SendMessage(const std::string &cszTopic,
                         const std::string &cszSendMsg,
                         int ervSendType,
                         const std::string &cszSendMsgBinary,
                         const std::string &cszSendMsgPPBODY,
                         const std::string &cszSendMsgDMS)
{
    if (!m_producer) {
        std::cerr << "Kafka producer is not initialized" << std::endl;
        return false;
    }

    // Kafka payload는 단일 메시지 바이트로 전달됩니다.
    // 필요 시 cszSendMsgBinary / cszSendMsgPPBODY / cszSendMsgDMS는 현재 별도 처리하지 않습니다.
    const std::string payload = !cszSendMsg.empty() ? cszSendMsg : cszSendMsgBinary;

    RdKafka::ErrorCode err = m_producer->produce(
        cszTopic.empty() ? m_topic : cszTopic,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char *>(payload.c_str()),
        payload.size(),
        nullptr,
        nullptr
    );

    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "Failed to produce Kafka message: " << RdKafka::err2str(err) << std::endl;
        return false;
    }

    m_producer->poll(0);
    return true;
}

bool CKafka::ReceiveMessage(std::string &cszMsg,
                            int &eRvReadType,
                            std::string &cszMsgBin,
                            int nTimeout)
{
    if (!m_consumer) {
        std::cerr << "Kafka consumer is not initialized" << std::endl;
        return false;
    }

    RdKafka::Message *message = m_consumer->consume(nTimeout);
    if (!message) {
        return false;
    }

    bool result = true;
    if (message->err()) {
        if (message->err() == RdKafka::ERR__TIMED_OUT) {
            result = false;
        } else if (message->err() == RdKafka::ERR__PARTITION_EOF) {
            result = false;
        } else {
            std::cerr << "Kafka consumer error: " << RdKafka::err2str(message->err()) << std::endl;
            result = false;
        }
    } else {
        const char *payload = static_cast<const char *>(message->payload());
        size_t len = message->len();
        cszMsg.assign(payload, len);
        cszMsgBin.clear();
        m_lastPayload = cszMsg;
        eRvReadType = 1; // 텍스트 메시지 수신 완료
    }

    delete message;
    return result;
}

bool CKafka::ReceiveMessage(std::string &cszMsg,
                            int &eRvReadType,
                            int nTimeout)
{
    std::string dummyBin;
    return ReceiveMessage(cszMsg, eRvReadType, dummyBin, nTimeout);
}

std::string CKafka::GetBinData(int nRvReadType) const
{
    // Kafka 전용에서는 마지막 페이로드를 반환합니다.
    return m_lastPayload;
}

std::string CKafka::GetBinBinary() const
{
    // Kafka는 RV 필드 구조가 없으므로 마지막 메시지를 그대로 반환합니다.
    return m_lastPayload;
}

std::string CKafka::GetBinPPBODY() const
{
    // Kafka에서는 PPBODY 필드가 별도 지원되지 않습니다.
    return std::string();
}

std::string CKafka::GetBinDMS() const
{
    // Kafka에서는 DMS 필드가 별도 지원되지 않습니다.
    return std::string();
}

std::string CKafka::GetReplySubject() const
{
    // Kafka에는 RV Reply Subject 개념이 없습니다.
    return std::string();
}

RdKafka::Producer* CKafka::GetProducer() const
{
    return m_producer;
}

RdKafka::KafkaConsumer* CKafka::GetConsumer() const
{
    return m_consumer;
}
