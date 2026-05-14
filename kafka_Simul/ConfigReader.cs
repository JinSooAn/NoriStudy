using System;
using System.Collections.Generic;
using System.IO;

public class ConfigReader
{
    private Dictionary<string, Dictionary<string, string>> _config = new();
    
    public ConfigReader(string filePath)
    {
        LoadConfig(filePath);
    }

    private void LoadConfig(string filePath)
    {
        if (!File.Exists(filePath))
        {
            throw new FileNotFoundException($"설정 파일을 찾을 수 없습니다: {filePath}");
        }

        string currentSection = null;
        foreach (var line in File.ReadAllLines(filePath))
        {
            var trimmed = line.Trim();
            
            // 빈 줄 또는 주석 무시
            if (string.IsNullOrEmpty(trimmed) || trimmed.StartsWith(";") || trimmed.StartsWith("#"))
                continue;

            // 섹션 처리
            if (trimmed.StartsWith("[") && trimmed.EndsWith("]"))
            {
                currentSection = trimmed.Substring(1, trimmed.Length - 2);
                if (!_config.ContainsKey(currentSection))
                {
                    _config[currentSection] = new Dictionary<string, string>();
                }
                continue;
            }

            // 키=값 처리
            if (currentSection != null && trimmed.Contains("="))
            {
                var parts = trimmed.Split('=', 2);
                if (parts.Length == 2)
                {
                    var key = parts[0].Trim();
                    var value = parts[1].Trim();
                    _config[currentSection][key] = value;
                }
            }
        }
    }

    public string GetValue(string section, string key, string defaultValue = "")
    {
        if (_config.TryGetValue(section, out var sectionDict))
        {
            if (sectionDict.TryGetValue(key, out var value))
            {
                return value;
            }
        }
        return defaultValue;
    }

    public int GetIntValue(string section, string key, int defaultValue = 0)
    {
        var value = GetValue(section, key);
        return int.TryParse(value, out var result) ? result : defaultValue;
    }
}
