import json
import os
import redis
from flask import Flask, jsonify

app = Flask(__name__)

REDIS_HOST = os.environ.get('REDIS_HOST', '127.0.0.1')
REDIS_PORT = int(os.environ.get('REDIS_PORT', 6379))
REDIS_PASSWORD = os.environ.get('REDISCLI_AUTH', None)
MODULES_DIR = os.environ.get('MODULES_DIR', 'Modules')


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


@app.route('/modules/list')
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
        module_json_path = os.path.join(MODULES_DIR, module_name, 'module.json')
        readme_path = os.path.join(MODULES_DIR, module_name, 'README.md')
        modules.append({
            'name': module_name,
            'path': module_path,
            'hasModuleJson': os.path.isfile(module_json_path),
            'hasReadme': os.path.isfile(readme_path)
        })

    return jsonify({
        'status': 'ok',
        'count': len(modules),
        'modules': modules
    })


@app.route('/modules/<name>')
def get_module_detail(name):
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

    module_key = f'{name}-module'
    if not r.exists(module_key):
        return jsonify({
            'status': 'error',
            'message': f'Module "{name}" not found in Redis'
        }), 404

    module_path = r.get(module_key)
    module_json_path = os.path.join(MODULES_DIR, name, 'module.json')
    readme_path = os.path.join(MODULES_DIR, name, 'README.md')

    module_json_data = None
    if os.path.isfile(module_json_path):
        try:
            with open(module_json_path, 'r', encoding='utf-8') as f:
                module_json_data = json.load(f)
        except (json.JSONDecodeError, OSError) as e:
            return jsonify({
                'status': 'error',
                'message': f'Failed to read module.json for "{name}": {str(e)}'
            }), 500

    readme_content = None
    if os.path.isfile(readme_path):
        try:
            with open(readme_path, 'r', encoding='utf-8') as f:
                readme_content = f.read()
        except OSError as e:
            return jsonify({
                'status': 'error',
                'message': f'Failed to read README.md for "{name}": {str(e)}'
            }), 500

    return jsonify({
        'status': 'ok',
        'name': name,
        'path': module_path,
        'moduleJson': module_json_data,
        'readme': readme_content
    })


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
