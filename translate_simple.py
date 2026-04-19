import os
import re
import requests

# Protected list of project names and terminology
PROTECTED_TERMS = [
    'Sunshine', 'README', 'GitHub', 'CI', 'API', 'Markdown', 'OpenAI', 'DeepL', 'Google Translate',
    # Add more terms here as needed
]

def mask_terms(text):
    for term in PROTECTED_TERMS:
        text = re.sub(rf'(?<![`\w]){re.escape(term)}(?![`\w])', f'@@@{term}@@@', text)
    return text

def unmask_terms(text):
    for term in PROTECTED_TERMS:
        text = text.replace(f'@@@{term}@@@', term)
    return text

def translate_with_deepseek(text, target_lang):
    # Translate using the DeepSeek API
    api_key = os.getenv('DEEPSEEK_API_KEY')
    if not api_key:
        raise Exception('DEEPSEEK_API_KEY environment variable is not set')
    url = 'https://api.deepseek.com/v1/chat/completions'
    prompt = f"Please translate the following Markdown content into {target_lang}, but do not translate the project name or these terms: {', '.join(PROTECTED_TERMS)}. Preserve the original formatting, links, and images.\n\n{text}"
    headers = {
        'Authorization': f'Bearer {api_key}',
        'Content-Type': 'application/json'
    }
    payload = {
        "model": "deepseek-chat",
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0.2
    }
    resp = requests.post(url, headers=headers, json=payload)
    resp.raise_for_status()
    result = resp.json()
    return result['choices'][0]['message']['content']

def translate_readme():
    with open('README.md', 'r', encoding='utf-8') as f:
        content = f.read()

    languages = [
        ('en', 'English'),
        ('fr', 'French'),
        ('de', 'German'),
        ('ja', 'Japanese')
    ]

    for lang_code, lang_name in languages:
        try:
            if lang_code == 'zh_CN':
                translated_content = content
            else:
                masked = mask_terms(content)
                translated = translate_with_deepseek(masked, lang_name)
                translated = unmask_terms(translated)
                # Strip the extra prefix DeepSeek returns; keep only the first Markdown heading and everything after it
                lines = translated.splitlines()
                for idx, line in enumerate(lines):
                    if line.strip().startswith('#'):
                      translated_content = '\n'.join(lines[idx:])
                      break
                else:
                  translated_content = translated.strip()

            filename = f'README.{lang_code}.md'
            with open(filename, 'w', encoding='utf-8') as f:
                f.write(translated_content)
            print(f"✓ Translated to {lang_name} ({lang_code})")
        except Exception as e:
            print(f"✗ Failed to translate to {lang_name}: {e}")

if __name__ == "__main__":
    translate_readme()
