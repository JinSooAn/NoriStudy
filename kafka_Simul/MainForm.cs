using System;
using System.Drawing;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using Confluent.Kafka;
using System.Text.Json;

public class MainForm : Form
{
    private ConfigReader config;
    private TextBox sendBrokerTextBox;
    private TextBox listenBrokerTextBox;
    private TextBox sendTopicTextBox;
    private TextBox receiveTopicTextBox;
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

    private CancellationTokenSource consumerCts;
    private CancellationTokenSource listenerCts;
    private IConsumer<Ignore, string> consumer;

    public MainForm()
    {
        Text = "Kafka Simulator";
        ClientSize = new Size(1000, 900);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.Sizable;
        MinimumSize = new Size(800, 700);
        MaximizeBox = true;
        AutoScaleMode = AutoScaleMode.Font;
        BackColor = Color.FromArgb(245, 245, 245);
        
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
        var titleLabel = new Label 
        { 
            Text = "Kafka Simulator", 
            Location = new Point(16, 16), 
            Size = new Size(500, 30),
            Font = new Font("Segoe UI", 18, FontStyle.Bold),
            ForeColor = Color.FromArgb(51, 51, 51),
            AutoSize = false
        };

        // Configuration Section
        var configGroupBox = new GroupBox 
        { 
            Text = "Configuration", 
            Location = new Point(16, 56), 
            Size = new Size(968, 80),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Segoe UI", 10, FontStyle.Bold)
        };

        var sendBrokerLabel = new Label { Text = "Send Broker:", Location = new Point(16, 24), AutoSize = true, Font = new Font("Segoe UI", 9) };
        sendBrokerTextBox = new TextBox 
        { 
            Location = new Point(120, 20), 
            Size = new Size(200, 24),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            Text = config.GetValue("Kafka", "SendBroker", config.GetValue("Kafka", "Broker", "localhost:9092")) 
        };

        var listenBrokerLabel = new Label { Text = "Listen Broker:", Location = new Point(340, 24), AutoSize = true, Font = new Font("Segoe UI", 9) };
        listenBrokerTextBox = new TextBox 
        { 
            Location = new Point(450, 20), 
            Size = new Size(200, 24),
            Anchor = AnchorStyles.Top | AnchorStyles.Right,
            Text = config.GetValue("Kafka", "ListenBroker", config.GetValue("Kafka", "Broker", "localhost:9092")) 
        };

        var sendTopicLabel = new Label { Text = "Send Topic:", Location = new Point(16, 56), AutoSize = true, Font = new Font("Segoe UI", 9) };
        sendTopicTextBox = new TextBox 
        { 
            Location = new Point(120, 52), 
            Size = new Size(200, 24),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            Text = config.GetValue("Kafka", "SendTopic", "send-topic") 
        };

        var receiveTopicLabel = new Label { Text = "Receive Topic:", Location = new Point(340, 56), AutoSize = true, Font = new Font("Segoe UI", 9) };
        receiveTopicTextBox = new TextBox 
        { 
            Location = new Point(450, 52), 
            Size = new Size(200, 24),
            Anchor = AnchorStyles.Top | AnchorStyles.Right,
            Text = config.GetValue("Kafka", "ReceiveTopic", "receive-topic") 
        };

        configGroupBox.Controls.Add(sendBrokerLabel);
        configGroupBox.Controls.Add(sendBrokerTextBox);
        configGroupBox.Controls.Add(listenBrokerLabel);
        configGroupBox.Controls.Add(listenBrokerTextBox);
        configGroupBox.Controls.Add(sendTopicLabel);
        configGroupBox.Controls.Add(sendTopicTextBox);
        configGroupBox.Controls.Add(receiveTopicLabel);
        configGroupBox.Controls.Add(receiveTopicTextBox);

        // Producer Group
        var producerGroup = new GroupBox 
        { 
            Text = "Producer", 
            Location = new Point(16, 152), 
            Size = new Size(460, 260),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Bottom,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            BackColor = Color.FromArgb(250, 250, 250)
        };
        messageTextBox = new TextBox 
        { 
            Multiline = true, 
            Location = new Point(16, 32), 
            Size = new Size(428, 150), 
            ScrollBars = ScrollBars.Vertical,
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Bottom,
            Font = new Font("Consolas", 9)
        };
        sendButton = new Button 
        { 
            Text = "Send Message", 
            Location = new Point(16, 194), 
            Size = new Size(200, 36),
            Anchor = AnchorStyles.Bottom | AnchorStyles.Left,
            BackColor = Color.FromArgb(33, 150, 243),
            ForeColor = Color.White,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            FlatStyle = FlatStyle.Flat,
            Cursor = Cursors.Hand
        };
        producerStatusLabel = new Label 
        { 
            Text = "Producer 상태: 준비", 
            Location = new Point(16, 240), 
            Size = new Size(428, 20),
            Anchor = AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Segoe UI", 9),
            ForeColor = Color.FromArgb(76, 175, 80)
        };
        sendButton.Click += SendButton_Click;
        producerGroup.Controls.Add(messageTextBox);
        producerGroup.Controls.Add(sendButton);
        producerGroup.Controls.Add(producerStatusLabel);

        // Consumer Group
        var consumerGroup = new GroupBox 
        { 
            Text = "Consumer", 
            Location = new Point(492, 152), 
            Size = new Size(492, 130),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            BackColor = Color.FromArgb(250, 250, 250)
        };
        startConsumerButton = new Button 
        { 
            Text = "▶ Start Consumer", 
            Location = new Point(16, 32), 
            Size = new Size(220, 36),
            Anchor = AnchorStyles.Top | AnchorStyles.Left,
            BackColor = Color.FromArgb(76, 175, 80),
            ForeColor = Color.White,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            FlatStyle = FlatStyle.Flat,
            Cursor = Cursors.Hand
        };
        stopConsumerButton = new Button 
        { 
            Text = "⏹ Stop Consumer", 
            Location = new Point(250, 32), 
            Size = new Size(220, 36),
            Enabled = false,
            Anchor = AnchorStyles.Top | AnchorStyles.Right,
            BackColor = Color.FromArgb(244, 67, 54),
            ForeColor = Color.White,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            FlatStyle = FlatStyle.Flat,
            Cursor = Cursors.Hand
        };
        consumerStatusLabel = new Label 
        { 
            Text = "Consumer 상태: 중지", 
            Location = new Point(16, 80), 
            Size = new Size(454, 20),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Segoe UI", 9),
            ForeColor = Color.FromArgb(244, 67, 54)
        };
        startConsumerButton.Click += StartConsumerButton_Click;
        stopConsumerButton.Click += StopConsumerButton_Click;
        consumerGroup.Controls.Add(startConsumerButton);
        consumerGroup.Controls.Add(stopConsumerButton);
        consumerGroup.Controls.Add(consumerStatusLabel);

        // Listener Group
        var listenerGroup = new GroupBox 
        { 
            Text = "Listener", 
            Location = new Point(492, 288), 
            Size = new Size(492, 130),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Bottom,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            BackColor = Color.FromArgb(250, 250, 250)
        };
        var portLabel = new Label { Text = "Port:", Location = new Point(16, 32), AutoSize = true, Font = new Font("Segoe UI", 9) };
        listenPortTextBox = new TextBox 
        { 
            Location = new Point(60, 28), 
            Size = new Size(100, 24),
            Text = "8080",
            Anchor = AnchorStyles.Top | AnchorStyles.Left
        };
        startListenerButton = new Button 
        { 
            Text = "▶ Start Listener", 
            Location = new Point(16, 68), 
            Size = new Size(220, 36),
            Anchor = AnchorStyles.Bottom | AnchorStyles.Left,
            BackColor = Color.FromArgb(76, 175, 80),
            ForeColor = Color.White,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            FlatStyle = FlatStyle.Flat,
            Cursor = Cursors.Hand
        };
        stopListenerButton = new Button 
        { 
            Text = "⏹ Stop Listener", 
            Location = new Point(250, 68), 
            Size = new Size(220, 36),
            Enabled = false,
            Anchor = AnchorStyles.Bottom | AnchorStyles.Right,
            BackColor = Color.FromArgb(244, 67, 54),
            ForeColor = Color.White,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            FlatStyle = FlatStyle.Flat,
            Cursor = Cursors.Hand
        };
        listenerStatusLabel = new Label 
        { 
            Text = "Listener 상태: 중지", 
            Location = new Point(16, 108), 
            Size = new Size(454, 20),
            Anchor = AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Segoe UI", 9),
            ForeColor = Color.FromArgb(244, 67, 54)
        };
        startListenerButton.Click += StartListenerButton_Click;
        stopListenerButton.Click += StopListenerButton_Click;
        listenerGroup.Controls.Add(portLabel);
        listenerGroup.Controls.Add(listenPortTextBox);
        listenerGroup.Controls.Add(startListenerButton);
        listenerGroup.Controls.Add(stopListenerButton);
        listenerGroup.Controls.Add(listenerStatusLabel);

        // Log Group
        var logGroup = new GroupBox 
        { 
            Text = "Message Log", 
            Location = new Point(16, 424), 
            Size = new Size(968, 460),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            BackColor = Color.FromArgb(250, 250, 250)
        };
        logListBox = new ListBox 
        { 
            Location = new Point(12, 28), 
            Size = new Size(944, 420),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            Font = new Font("Consolas", 9),
            BackColor = Color.FromArgb(33, 33, 33),
            ForeColor = Color.FromArgb(0, 255, 0)
        };
        logGroup.Controls.Add(logListBox);

        Controls.Add(titleLabel);
        Controls.Add(configGroupBox);
        Controls.Add(producerGroup);
        Controls.Add(consumerGroup);
        Controls.Add(listenerGroup);
        Controls.Add(logGroup);
    }

    private async void SendButton_Click(object sender, EventArgs e)
    {
        var broker = sendBrokerTextBox.Text.Trim();
        var topic = sendTopicTextBox.Text.Trim();
        var message = messageTextBox.Text.Trim();

        if (string.IsNullOrEmpty(broker) || string.IsNullOrEmpty(topic))
        {
            MessageBox.Show("Broker와 Send Topic을 모두 입력해주세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (string.IsNullOrEmpty(message))
        {
            MessageBox.Show("전송할 메시지를 입력하세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        sendButton.Enabled = false;
        producerStatusLabel.Text = "Producer 상태: 전송 중...";
        var jsonMessage = JsonSerializer.Serialize(new { message = message });

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
                var result = await producer.ProduceAsync(topic, new Message<Null, string> { Value = jsonMessage });
                AddLog($"[SENT] Topic: {topic}, Partition: {result.Partition}, Offset: {result.Offset.Value}");
                ShowJsonPopup("📤 Sent Message", jsonMessage);
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

    private void StartConsumerButton_Click(object sender, EventArgs e)
    {
        var broker = listenBrokerTextBox.Text.Trim();
        var topic = receiveTopicTextBox.Text.Trim();

        if (string.IsNullOrEmpty(broker) || string.IsNullOrEmpty(topic))
        {
            MessageBox.Show("Broker와 Receive Topic을 모두 입력해주세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
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

    private void StopConsumerButton_Click(object sender, EventArgs e)
    {
        if (consumerCts == null)
            return;

        consumerCts.Cancel();
        stopConsumerButton.Enabled = false;
        consumerStatusLabel.Text = "Consumer 상태: 중지 요청...";
    }

    private void StartListenerButton_Click(object sender, EventArgs e)
    {
        var broker = listenBrokerTextBox.Text.Trim();
        var topic = receiveTopicTextBox.Text.Trim();
        var port = listenPortTextBox.Text.Trim();

        if (string.IsNullOrEmpty(broker) || string.IsNullOrEmpty(topic))
        {
            MessageBox.Show("Broker와 Receive Topic을 모두 입력해주세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
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

    private void StopListenerButton_Click(object sender, EventArgs e)
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
        var maxItems = config.GetIntValue("UI", "DefaultMessageCount", 200);
        if (logListBox.Items.Count > maxItems)
        {
            logListBox.Items.RemoveAt(logListBox.Items.Count - 1);
        }
    }

    private void ShowJsonPopup(string title, string jsonMessage)
    {
        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => ShowJsonPopup(title, jsonMessage)));
            return;
        }

        var form = new Form
        {
            Text = title,
            Size = new Size(600, 400),
            StartPosition = FormStartPosition.CenterParent,
            Font = new Font("Segoe UI", 10),
            BackColor = Color.FromArgb(245, 245, 245)
        };

        var textBox = new TextBox
        {
            Multiline = true,
            ReadOnly = true,
            Dock = DockStyle.Fill,
            Font = new Font("Consolas", 10),
            BackColor = Color.FromArgb(33, 33, 33),
            ForeColor = Color.FromArgb(0, 255, 0),
            Text = FormatJson(jsonMessage)
        };

        form.Controls.Add(textBox);
        form.ShowDialog(this);
    }

    private string FormatJson(string json)
    {
        try
        {
            using (var doc = System.Text.Json.JsonDocument.Parse(json))
            {
                var options = new System.Text.Json.JsonSerializerOptions { WriteIndented = true };
                return System.Text.Json.JsonSerializer.Serialize(doc.RootElement, options);
            }
        }
        catch
        {
            return json;
        }
    }

    private void RunConsumer(string broker, string topic, CancellationToken token)
    {
        var groupId = config.GetValue("Kafka", "GroupId", "kafka-simul-ui-group") ?? "kafka-simul-ui-group";
        var autoOffsetReset = config.GetValue("UI", "AutoOffsetReset", "Earliest") ?? "Earliest";

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
                        AddLog($"[RECEIVED] Topic: {topic}, Partition: {consumeResult.Partition}, Offset: {consumeResult.Offset}");
                        ShowJsonPopup("📥 Received Message (Consumer)", consumeResult.Message.Value);
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
        var groupId = config.GetValue("Kafka", "GroupId", "kafka-simul-listener-group") ?? "kafka-simul-listener-group";

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
                        AddLog($"[RECEIVED] Port: {port}, Topic: {topic}, Partition: {consumeResult.Partition}, Offset: {consumeResult.Offset}");
                        ShowJsonPopup($"📥 Received Message (Listener Port {port})", consumeResult.Message.Value);
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

