FROM docker.pisugar.uk/python:3.12-slim
WORKDIR /app
ADD . /app

RUN pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple

CMD ["python", "server.py"]
# CMD ["gunicorn","servver:app","-c","gunicorn.conf.py"]
# ENTRYPOINT ["/app/twoservers.sh"] 
