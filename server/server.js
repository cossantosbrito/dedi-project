'use strict';

require('dotenv').config();
const express = require('express');
const cors = require('cors');
const apiRouter = require('./api');
const { startIngest } = require('./ingest');

const app = express();
app.use(cors());
app.use('/api', apiRouter);

const port = Number(process.env.HTTP_PORT || 8090);
app.listen(port, function () {
  console.log('API dashboard iha http://localhost:' + port + '/api');
});

startIngest();
