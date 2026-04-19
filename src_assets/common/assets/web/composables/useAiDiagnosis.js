import { ref, reactive } from 'vue'

const STORAGE_KEY = 'sunshine-ai-diagnosis-config'

const PROVIDERS = [
  { label: 'OpenAI', value: 'openai', base: 'https://api.openai.com/v1', models: ['gpt-4o-mini', 'gpt-4o'] },
  { label: 'DeepSeek', value: 'deepseek', base: 'https://api.deepseek.com/v1', models: ['deepseek-chat'] },
  { label: 'Qwen (Tongyi)', value: 'qwen', base: 'https://dashscope.aliyuncs.com/compatible-mode/v1', models: ['qwen-plus', 'qwen-turbo'] },
  { label: 'Zhipu (GLM)', value: 'glm', base: 'https://open.bigmodel.cn/api/paas/v4', models: ['glm-4-flash', 'glm-4'] },
  { label: 'OpenRouter', value: 'openrouter', base: 'https://openrouter.ai/api/v1', models: ['deepseek/deepseek-chat-v3-0324', 'google/gemini-2.0-flash-001'] },
  { label: 'Ollama (local)', value: 'ollama', base: 'http://localhost:11434/v1', models: ['llama3', 'qwen2'] },
  { label: 'Custom', value: 'custom', base: '', models: [] },
]

const SYSTEM_PROMPT = `You are a log diagnostics assistant for the Sunshine streaming software. The user will provide Sunshine runtime logs; please analyze them and provide a diagnosis.

Please focus on:
- **Fatal/Error level logs**: usually the direct cause of the problem
- **Warning logs**: may hint at underlying issues
- **Encoder-related**: NVENC/AMF/software-encoding errors or fallbacks
- **Network/connection**: Moonlight client connection failures, timeouts, pairing issues
- **Audio/video pipeline**: audio device problems, video capture failures
- **Configuration loading**: invalid or conflicting configuration entries

Diagnosis format:
1. **Issue summary**: a one-sentence summary of the issue
2. **Detailed analysis**: explain the cause of the problem (cite specific log lines)
3. **Recommended fix**: provide concrete, actionable steps
4. If no obvious errors are present in the logs, tell the user that everything is running normally

Please reply in clear and concise English.`

function loadConfig() {
  try {
    const saved = localStorage.getItem(STORAGE_KEY)
    if (saved) {
      const parsed = JSON.parse(saved)
      return { provider: 'openai', apiKey: '', apiBase: 'https://api.openai.com/v1', model: 'gpt-4o-mini', ...parsed }
    }
  } catch { /* ignore */ }
  return { provider: 'openai', apiKey: '', apiBase: 'https://api.openai.com/v1', model: 'gpt-4o-mini' }
}

export function useAiDiagnosis() {
  const config = reactive(loadConfig())
  const isLoading = ref(false)
  const result = ref('')
  const error = ref('')

  function saveConfig() {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ ...config }))
  }

  function onProviderChange(value) {
    const p = PROVIDERS.find((x) => x.value === value)
    if (p) {
      config.apiBase = p.base
      if (p.models.length > 0) config.model = p.models[0]
    }
  }

  function getAvailableModels() {
    const p = PROVIDERS.find((x) => x.value === config.provider)
    return p?.models || []
  }

  async function diagnose(logs) {
    if (!logs) {
      error.value = 'No log content available'
      return
    }
    if (!config.apiKey && config.provider !== 'ollama') {
      error.value = 'Please configure an API key first'
      return
    }
    if (config.provider === 'custom') {
      try {
        const url = new URL(config.apiBase)
        if (!['http:', 'https:'].includes(url.protocol)) {
          throw new Error('invalid protocol')
        }
      } catch {
        error.value = 'Custom providers require a full API URL (starting with http:// or https://)'
        return
      }
    }

    saveConfig()
    isLoading.value = true
    result.value = ''
    error.value = ''

    // Truncate logs to last 200 lines to fit token limits
    const lines = logs.split('\n')
    const truncated = lines.slice(-200).join('\n')

    try {
      const base = config.apiBase.replace(/\/+$/, '')
      const headers = { 'Content-Type': 'application/json' }
      if (config.apiKey) headers['Authorization'] = `Bearer ${config.apiKey}`

      const resp = await fetch(`${base}/chat/completions`, {
        method: 'POST',
        headers,
        body: JSON.stringify({
          model: config.model,
          messages: [
            { role: 'system', content: SYSTEM_PROMPT },
            { role: 'user', content: `Please analyze the following Sunshine logs:\n\n\`\`\`\n${truncated}\n\`\`\`` },
          ],
          temperature: 0.3,
          max_tokens: 2048,
        }),
      })

      if (!resp.ok) {
        const text = await resp.text()
        throw new Error(`API request failed (${resp.status}): ${text.substring(0, 200)}`)
      }

      const data = await resp.json()
      result.value = data.choices?.[0]?.message?.content || 'Unable to fetch analysis result'
    } catch (e) {
      error.value = e.message
    } finally {
      isLoading.value = false
    }
  }

  return {
    config,
    providers: PROVIDERS,
    isLoading,
    result,
    error,
    onProviderChange,
    getAvailableModels,
    diagnose,
    saveConfig,
  }
}
