import os
import redis
from flask import Flask, jsonify

app = Flask(__name__)

REDIS_HOST = os.environ.get('REDIS_HOST', '127.0.0.1')
REDIS_PORT = int(os.environ.get('REDIS_PORT', 6379))
REDIS_PASSWORD = os.environ.get('REDISCLI_AUTH', None)


def get_redis_client():
    return redis.Redis(
        host=REDIS_HOST,
        port=REDIS_PORT,
        password=REDIS_PASSWORD,
        decode_responses=True
    )


@app.route('/')
def index():
    return jsonify({
        'status': 'ok',
        'message': 'Computing server API is running'
    })


@app.route('/modules')
def get_modules():
    try:
        r = get_redis_client()
        r.ping()
    except redis.ConnectionError as e:
        return jsonify({
            'status': 'error',
            'message': f'Failed to connect to Redis: {str(e)}'
        }), 503
    except redis.AuthenticationError as e:
        return jsonify({
            'status': 'error',
            'message': f'Redis authentication failed: {str(e)}'
        }), 503

    keys = r.keys('*-module')
    modules = []
    for key in keys:
        module_name = key[:-len('-module')]
        module_path = r.get(key)
        modules.append({
            'name': module_name,
            'path': module_path
        })

    return jsonify({
        'status': 'ok',
        'count': len(modules),
        'modules': modules
    })


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
