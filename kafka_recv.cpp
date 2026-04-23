#include <iostream>
#include <string>
#include <signal.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <librdkafka/rdkafkacpp.h>

static volatile int run = 1;

static void sig_handler(int sig) {
    run = 0;
}

// 현재 시간을 문자열로 반환
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// 로그 파일에 메시지 기록
void writeLog(const std::string &message) {
    std::string log_dir = "logs";
    std::string log_file = log_dir + "/kafka_messages.log";
    
    // logs 폴더 생성 시도
    system("mkdir -p logs 2>/dev/null");
    
    // 파일에 기록 (append 모드)
    std::ofstream outfile(log_file, std::ios::app);
    if (outfile.is_open()) {
        outfile << "[" << getCurrentTime() << "] " << message << std::endl;
        outfile.close();
    } else {
        std::cerr << "Failed to write log file" << std::endl;
    }
}

class ExampleConsumeCb : public RdKafka::ConsumeCb {
public:
    void consume_cb(RdKafka::Message &message, void *opaque) {
        // not used in this example
    }
};

int main(int argc, char *argv[]) {
    std::string brokers = "localhost:9092";
    std::string topic = "test-topic";
    std::string group_id = "test-group";
    std::string errstr;
    
    // Kafka 설정
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    
    // Broker 설정
    if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set bootstrap.servers: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    // 그룹 ID 설정
    if (conf->set("group.id", group_id, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set group.id: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    // Auto commit 설정
    if (conf->set("enable.auto.commit", "true", errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set enable.auto.commit: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    // Earliest offset부터 시작
    if (conf->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set auto.offset.reset: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    // Topic 설정 생성
    RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
    
    // 컨슈머 생성
    RdKafka::KafkaConsumer *consumer = RdKafka::KafkaConsumer::create(conf, errstr);
    if (!consumer) {
        std::cerr << "Failed to create consumer: " << errstr << std::endl;
        delete conf;
        return 1;
    }
    
    delete conf;
    
    // 토픽 구독
    std::vector<std::string> topics;
    topics.push_back(topic);
    
    RdKafka::ErrorCode err = consumer->subscribe(topics);
    if (err) {
        std::cerr << "Failed to subscribe to topic: " << RdKafka::err2str(err) << std::endl;
        delete consumer;
        return 1;
    }
    
    // Ctrl+C 처리
    signal(SIGINT, sig_handler);
    
    std::cout << "Kafka Consumer Started" << std::endl;
    std::cout << "Topic: " << topic << std::endl;
    std::cout << "Group: " << group_id << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << "---" << std::endl;
    
    int msg_count = 0;
    
    while (run) {
        RdKafka::Message *msg = consumer->consume(1000);
        
        if (msg) {
            if (msg->err()) {
                if (msg->err() == RdKafka::ERR__TIMED_OUT) {
                    // Timeout, no message
                } else if (msg->err() == RdKafka::ERR__PARTITION_EOF) {
                    std::cout << "Reached end of partition" << std::endl;
                } else {
                    std::cerr << "Consumer error: " << RdKafka::err2str(msg->err()) << std::endl;
                }
            } else {
                msg_count++;
                std::string payload((char *)msg->payload(), msg->len());
                std::cout << "[" << msg_count << "] Received: " << payload << std::endl;
                
                // 로그 파일에 기록
                writeLog(payload);
            }
            
            delete msg;
        }
    }
    
    std::cout << std::endl << "Stopping consumer..." << std::endl;
    
    // 구독 해제
    consumer->unsubscribe();
    
    delete consumer;
    
    std::cout << "Consumer closed. Total messages received: " << msg_count << std::endl;
    
    return 0;
}
