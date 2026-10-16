FROM python:3.12.7-alpine3.20
RUN pip install --no-cache-dir websockets==13.1
WORKDIR /app
COPY server.py .
USER 1000:1000
CMD ["python", "-u", "server.py"]
