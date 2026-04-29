# NoriStudy
Ai Study

## Kafka DRT Configuration

The new `DRT_Comm` path uses Kafka via `CKafka` and reads settings from the INI file.

Expected configuration keys:

- `DRT/BROKER` - Kafka bootstrap broker list, e.g. `localhost:9092`
- `DRT/TOPIC` - topic name for DRT messages
- `DRT/GROUPID` - consumer group ID for replies or future use

If your application uses `CIniFile`, add these settings to the appropriate section in your configuration file.
