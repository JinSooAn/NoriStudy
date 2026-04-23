#include <iostream>
#include <string>
#include <librdkafka/rdkafkacpp.h>

class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message &message) {
        if (message.err()) {
            std::cerr << "Message delivery failed: " << RdKafka::err2str(message.err()) << std::endl;
        } else {
            std::cout << "Message delivered to topic " << message.topic_name()
                      << " [" << message.partition() << "] at offset "
                      << message.offset() << std::endl;
        }
    }
};

int main(int argc, char *argv[]) {
    // Argument 확인
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <message>" << std::endl;
        std::cerr << "Example: " << argv[0] << " \"Hello Kafka\"" << std::endl;
        return 1;
    }
    
    std::string brokers = "localhost:9092";
    std::string topic = "test-topic";
    std::string errstr;
    std::string message = argv[1];
    
    // Kafka 설정
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    ExampleDeliveryReportCb dr_cb;
    
    // Broker 설정
    if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set bootstrap.servers: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    // Delivery callback 설정
    if (conf->set("dr_cb", &dr_cb, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set dr_cb: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    // 프로듀서 생성
    RdKafka::Producer *producer = RdKafka::Producer::create(conf, errstr);
    if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    delete conf;
    
    // 메시지 전송
    RdKafka::ErrorCode err = producer->produce(
        topic,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        (void *)message.c_str(),
        message.length(),
        NULL,
        0,
        NULL,
        NULL
    );
    
    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "Failed to produce message: " << RdKafka::err2str(err) << std::endl;
        delete producer;
        return 1;
    }
    
    std::cout << "[Sent] " << message << std::endl;
    
    // Poll to handle callbacks
    producer->poll(100);
    
    std::cout << "Waiting for message delivery..." << std::endl;
    producer->flush(30 * 1000);
    
    delete producer;
    
    return 0;
}
