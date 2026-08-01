# syntax=docker/dockerfile:1

ARG PYTHON_VERSION=3.14.4
FROM python:${PYTHON_VERSION}-slim-bookworm

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

WORKDIR /app

COPY requirements-runtime.txt ./
RUN python -m pip install \
        --no-cache-dir \
        --no-compile \
        --requirement requirements-runtime.txt \
    && groupadd --gid 10001 app \
    && useradd \
        --uid 10001 \
        --gid app \
        --no-create-home \
        --shell /usr/sbin/nologin \
        app \
    && install --directory --owner app --group app /data

COPY --chown=app:app backend ./backend

USER app

EXPOSE 8000

CMD ["python", "-m", "uvicorn", "backend.api.main:app", "--host", "0.0.0.0", "--port", "8000"]
