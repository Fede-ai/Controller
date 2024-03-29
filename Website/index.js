const http = require('http');
const fs = require('fs');
const path = require('path');
const port = 8000;

http.createServer((req, res) => {
  let filePath = req.url;
  if (filePath === '/')
    filePath = 'index.html';
	filePath = path.join(__dirname, filePath);

  const extname = path.extname(filePath);
  let contentType = 'text/html';

  if (extname === '.css')
    contentType = 'text/css';

  fs.readFile(filePath, (err, content) => {
		if (err) {
			if (err.code === 'ENOENT') {
				res.writeHead(404);
				res.end('404 Not Found');
			} else {
				res.writeHead(500);
				res.end('500 Internal Server Error: ' + err.code);
			}
		} else {
			res.writeHead(200, { 'Content-Type': contentType });
			res.end(content, 'utf-8');
		}
  });
}).listen(port, () => console.log(`Server is running on port ${port}`));