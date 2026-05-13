using System;
using System.Drawing;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using Confluent.Kafka;

public class MainForm : Form
{
    private ConfigReader? config;
    private TextBox brokerTextBox;
    private TextBox topicTextBox;
    private TextBox messageTextBox;
    private TextBox listenPortTextBox;
    private Button sendButton;
    private Button startConsumerButton;
    private Button stopConsumerButton;
    private Button startListenerButton;
    private Button stopListenerButton;
    private ListBox logListBox;
    private Label producerStatusLabel;
    private Label consumerStatusLabel;
    private Label listenerStatusLabel;

    private CancellationTokenSource? consumerCts;
    private CancellationTokenSource? listenerCts;
    private IConsumer<Ignore, string>? consumer;

    public MainForm()
    {
        Text = "Kafka Simulator UI";
        ClientSize = new Size(760, 700);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        
        try
        {
            config = new ConfigReader("config.ini");
        }
        catch (Exception ex)
        {
            MessageBox.Show($"설정 파일 오류: {ex.Message}", "오류", MessageBoxButtons.OK, MessageBoxIcon.Error);
            config = new ConfigReader("dummy");
        }
        
        InitializeComponents();
    }

    private void InitializeComponents()
    {
        var brokerLabel = new Label { Text = "Broker:", Location = new Point(16, 16), AutoSize = true };
        brokerTextBox = new TextBox 
        { 
            Location = new Point(80, 12), 
            Size = new Size(660, 24), 
            Text = config?.GetValue("Kafka", "Broker", "localhost:9092") ?? "localhost:9092" 
        };

        var topicLabel = new Label { Text = "Topic:", Location = new Point(16, 52), AutoSize = true };
        topicTextBox = new TextBox 
        { 
            Location = new Point(80, 48), 
            Size = new Size(660, 24), 
            Text = config?.GetValue("Kafka", "Topic", "test-topic") ?? "test-topic" 
        };

        // Producer Group
        var producerGroup = new GroupBox { Text = "Producer", Location = new Point(16, 92), Size = new Size(360, 220) };
        messageTextBox = new TextBox { Multiline = true, Location = new Point(16, 32), Size = new Size(328, 120), ScrollBars = ScrollBars.Vertical };
        sendButton = new Button { Text = "Send", Location = new Point(16, 164), Size = new Size(120, 32) };
        producerStatusLabel = new Label { Text = "Producer 상태: 준비", Location = new Point(16, 206), Size = new Size(328, 24) };
        sendButton.Click += SendButton_Click;
        producerGroup.Controls.Add(messageTextBox);
        producerGroup.Controls.Add(sendButton);
        producerGroup.Controls.Add(producerStatusLabel);

        // Consumer Group
        var consumerGroup = new GroupBox { Text = "Consumer", Location = new Point(392, 92), Size = new Size(360, 220) };
        startConsumerButton = new Button { Text = "Start Consumer", Location = new Point(16, 32), Size = new Size(150, 32) };
        stopConsumerButton = new Button { Text = "Stop Consumer", Location = new Point(190, 32), Size = new Size(150, 32), Enabled = false };
        consumerStatusLabel = new Label { Text = "Consumer 상태: 중지", Location = new Point(16, 76), Size = new Size(328, 24) };
        startConsumerButton.Click += StartConsumerButton_Click;
        stopConsumerButton.Click += StopConsumerButton_Click;
        consumerGroup.Controls.Add(startConsumerButton);
        consumerGroup.Controls.Add(stopConsumerButton);
        consumerGroup.Controls.Add(consumerStatusLabel);

        // Listener Group
        var listenerGroup = new GroupBox { Text = "Listener", Location = new Point(16, 328), Size = new Size(360, 120) };
        var portLabel = new Label { Text = "Port:", Location = new Point(16, 24), AutoSize = true };
        listenPortTextBox = new TextBox { Location = new Point(60, 20), Size = new Size(280, 24), Text = "8080" };
        startListenerButton = new Button { Text = "Start Listener", Location = new Point(16, 56), Size = new Size(150, 32) };
        stopListenerButton = new Button { Text = "Stop Listener", Location = new Point(190, 56), Size = new Size(150, 32), Enabled = false };
        listenerStatusLabel = new Label { Text = "Listener 상태: 중지", Location = new Point(16, 96), Size = new Size(328, 24) };
        startListenerButton.Click += StartListenerButton_Click;
        stopListenerButton.Click += StopListenerButton_Click;
        listenerGroup.Controls.Add(portLabel);
        listenerGroup.Controls.Add(listenPortTextBox);
        listenerGroup.Controls.Add(startListenerButton);
        listenerGroup.Controls.Add(stopListenerButton);
        listenerGroup.Controls.Add(listenerStatusLabel);

        // Log Group
        var logGroup = new GroupBox { Text = "Message Log", Location = new Point(16, 464), Size = new Size(736, 216) };
        logListBox = new ListBox { Location = new Point(12, 28), Size = new Size(712, 176) };
        logGroup.Controls.Add(logListBox);

        Controls.Add(brokerLabel);
        Controls.Add(brokerTextBox);
        Controls.Add(topicLabel);
        Controls.Add(topicTextBox);
        Controls.Add(producerGroup);
        Controls.Add(consumerGroup);
        Controls.Add(listenerGroup);
        Controls.Add(logGroup);
    }

    private async void SendButton_Click(object? sender, EventArgs e)
    {
        var broker = brokerTextBox.Text.Trim();
        var topic = topicTextBox.Text.Trim();
        var message = messageTextBox.Text.Trim();

        if (string.IsNullOrEmpty(broker) || string.IsNullOrEmpty(topic))
        {
            MessageBox.Show("Broker와 Topic을 모두 입력해주세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (string.IsNullOrEmpty(message))
        {
            MessageBox.Show("전송할 메시지를 입력하세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        sendButton.Enabled = false;
        producerStatusLabel.Text = "Producer 상태: 전송 중...";

        try
        {
            await Task.Run(async () =>
            {
                var config = new ProducerConfig
                {
                    BootstrapServers = broker,
                    Acks = Acks.All,
                    MessageTimeoutMs = 5000
                };

                using var producer = new ProducerBuilder<Null, string>(config).Build();
                var result = await producer.ProduceAsync(topic, new Message<Null, string> { Value = message });
                AddLog($"Sent: {message} ({result.TopicPartitionOffset})");
            });
        }
        catch (Exception ex)
        {
            AddLog($"Producer 오류: {ex.Message}");
        }
        finally
        {
            sendButton.Enabled = true;
            producerStatusLabel.Text = "Producer 상태: 준비";
        }
    }

    private void StartConsumerButton_Click(object? sender, EventArgs e)
    {
        var broker = brokerTextBox.Text.Trim();
        var topic = topicTextBox.Text.Trim();

        if (string.IsNullOrEmpty(broker) || string.IsNullOrEmpty(topic))
        {
            MessageBox.Show("Broker와 Topic을 모두 입력해주세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (consumerCts != null)
        {
            return;
        }

        consumerCts = new CancellationTokenSource();
        startConsumerButton.Enabled = false;
        stopConsumerButton.Enabled = true;
        consumerStatusLabel.Text = "Consumer 상태: 수신 중...";

        Task.Run(() => RunConsumer(broker, topic, consumerCts.Token));
    }

    private void StopConsumerButton_Click(object? sender, EventArgs e)
    {
        if (consumerCts == null)
            return;

        consumerCts.Cancel();
        stopConsumerButton.Enabled = false;
        consumerStatusLabel.Text = "Consumer 상태: 중지 요청...";
    }

    private void StartListenerButton_Click(object? sender, EventArgs e)
    {
        var broker = brokerTextBox.Text.Trim();
        var topic = topicTextBox.Text.Trim();
        var port = listenPortTextBox.Text.Trim();

        if (string.IsNullOrEmpty(broker) || string.IsNullOrEmpty(topic))
        {
            MessageBox.Show("Broker와 Topic을 모두 입력해주세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (!int.TryParse(port, out var portNum) || portNum <= 0 || portNum > 65535)
        {
            MessageBox.Show("올바른 포트번호를 입력하세요. (1-65535)", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (listenerCts != null)
        {
            return;
        }

        listenerCts = new CancellationTokenSource();
        startListenerButton.Enabled = false;
        stopListenerButton.Enabled = true;
        listenPortTextBox.Enabled = false;
        listenerStatusLabel.Text = $"Listener 상태: Listen 중 (Port: {portNum})...";

        Task.Run(() => RunListener(broker, topic, portNum, listenerCts.Token));
    }

    private void StopListenerButton_Click(object? sender, EventArgs e)
    {
        if (listenerCts == null)
            return;

        listenerCts.Cancel();
        stopListenerButton.Enabled = false;
        listenerStatusLabel.Text = "Listener 상태: 중지 요청...";
    }

    private void AddLog(string message)
    {
        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => AddLog(message)));
            return;
        }

        logListBox.Items.Insert(0, $"[{DateTime.Now:HH:mm:ss}] {message}");
        var maxItems = config?.GetIntValue("UI", "DefaultMessageCount", 200) ?? 200;
        if (logListBox.Items.Count > maxItems)
        {
            logListBox.Items.RemoveAt(logListBox.Items.Count - 1);
        }
    }

    private void RunConsumer(string broker, string topic, CancellationToken token)
    {
        var groupId = config?.GetValue("Kafka", "GroupId", "kafka-simul-ui-group") ?? "kafka-simul-ui-group";
        var autoOffsetReset = config?.GetValue("UI", "AutoOffsetReset", "Earliest") ?? "Earliest";

        var offsetReset = autoOffsetReset.Equals("Latest", StringComparison.OrdinalIgnoreCase) 
            ? AutoOffsetReset.Latest 
            : AutoOffsetReset.Earliest;

        var consumerConfig = new ConsumerConfig
        {
            BootstrapServers = broker,
            GroupId = groupId,
            AutoOffsetReset = offsetReset,
            EnableAutoCommit = true,
            EnablePartitionEof = false
        };

        using var consumerLocal = new ConsumerBuilder<Ignore, string>(consumerConfig).Build();
        consumer = consumerLocal;
        consumer.Subscribe(topic);

        AddLog($"Consumer started. Topic={topic}, GroupId={groupId}");

        try
        {
            while (!token.IsCancellationRequested)
            {
                try
                {
                    var consumeResult = consumer.Consume(token);
                    if (consumeResult != null)
                    {
                        AddLog($"[Consumer] Received: {consumeResult.Message.Value}");
                    }
                }
                catch (ConsumeException ex)
                {
                    AddLog($"Consumer 오류: {ex.Error.Reason}");
                }
            }
        }
        catch (OperationCanceledException)
        {
            AddLog("Consumer 취소됨.");
        }
        finally
        {
            try
            {
                consumer?.Close();
                AddLog("Consumer 종료됨.");
            }
            catch (Exception ex)
            {
                AddLog($"Consumer 종료 오류: {ex.Message}");
            }

            consumerCts = null;
            consumer = null;
            if (InvokeRequired)
            {
                BeginInvoke(new Action(() =>
                {
                    startConsumerButton.Enabled = true;
                    stopConsumerButton.Enabled = false;
                    consumerStatusLabel.Text = "Consumer 상태: 중지";
                }));
            }
            else
            {
                startConsumerButton.Enabled = true;
                stopConsumerButton.Enabled = false;
                consumerStatusLabel.Text = "Consumer 상태: 중지";
            }
        }
    }

    private void RunListener(string broker, string topic, int port, CancellationToken token)
    {
        var groupId = config?.GetValue("Kafka", "GroupId", "kafka-simul-listener-group") ?? "kafka-simul-listener-group";

        var consumerConfig = new ConsumerConfig
        {
            BootstrapServers = broker,
            GroupId = groupId,
            AutoOffsetReset = AutoOffsetReset.Latest,
            EnableAutoCommit = true,
            EnablePartitionEof = false
        };

        using var consumerLocal = new ConsumerBuilder<Ignore, string>(consumerConfig).Build();
        consumerLocal.Subscribe(topic);

        AddLog($"Listener started on port {port}. Topic={topic}");

        try
        {
            while (!token.IsCancellationRequested)
            {
                try
                {
                    var consumeResult = consumerLocal.Consume(token);
                    if (consumeResult != null)
                    {
                        AddLog($"[Listener] Received (Port {port}): {consumeResult.Message.Value}");
                    }
                }
                catch (ConsumeException ex)
                {
                    AddLog($"Listener 오류: {ex.Error.Reason}");
                }
            }
        }
        catch (OperationCanceledException)
        {
            AddLog("Listener 취소됨.");
        }
        finally
        {
            try
            {
                consumerLocal?.Close();
                AddLog($"Listener 종료됨. (Port {port})");
            }
            catch (Exception ex)
            {
                AddLog($"Listener 종료 오류: {ex.Message}");
            }

            listenerCts = null;
            if (InvokeRequired)
            {
                BeginInvoke(new Action(() =>
                {
                    startListenerButton.Enabled = true;
                    stopListenerButton.Enabled = false;
                    listenPortTextBox.Enabled = true;
                    listenerStatusLabel.Text = "Listener 상태: 중지";
                }));
            }
            else
            {
                startListenerButton.Enabled = true;
                stopListenerButton.Enabled = false;
                listenPortTextBox.Enabled = true;
                listenerStatusLabel.Text = "Listener 상태: 중지";
            }
        }
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        consumerCts?.Cancel();
        listenerCts?.Cancel();
        base.OnFormClosing(e);
    }
}

