const fs = require('fs');
const logPath = 'C:\\Users\\Admin\\.gemini\\antigravity\\brain\\e9161f7f-848e-4843-a28c-0b70bc23f194\\.system_generated\\logs\\transcript_full.jsonl';
const fileContent = fs.readFileSync(logPath, 'utf8');
const lines = fileContent.split('\n');

for (const line of lines) {
  if (!line.trim()) continue;
  try {
    const obj = JSON.parse(line);
    if (obj.step_index === 270) {
      console.log('Found step 270!');
      fs.writeFileSync('diff_recovered.txt', obj.content, 'utf8');
      console.log('Saved to diff_recovered.txt');
      break;
    }
  } catch (e) {
  }
}
