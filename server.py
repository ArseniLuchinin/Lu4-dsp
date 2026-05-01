import json
import os
import redis
import subprocess
from datetime import datetime
from dotenv import load_dotenv
from flask import Flask, jsonify, request

load_dotenv()

app = Flask(__name__)

SERVER_PORT = int(os.environ.get('SERVER_PORT', '5000'))
COMPUTING_SERVER_PATH = os.environ.get('COMPUTING_SERVER_PATH', './build/computing_server')
SESSIONS_DIR = os.environ.get('SESSIONS_DIR', './sessions')

REDIS_HOST = os.environ.get('REDIS_HOST', '127.0.0.1')
REDIS_PORT = int(os.environ.get('REDIS_PORT', '6379'))
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
        module_json_path = os.path.join(MODULES_DIR, module_name, 'module.json')
        readme_path = os.path.join(MODULES_DIR, module_name, 'README.md')
        modules.append({
            'name': module_name,
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
        'moduleJson': module_json_data,
        'readme': readme_content
    })


@app.route('/start', methods=['POST'])
def start():
    data = request.get_json(silent=True) or {}

    pipeline_raw = data.get('pipeline')
    variables_raw = data.get('variables')

    if pipeline_raw is None or variables_raw is None:
        return jsonify({
            'status': 'error',
            'message': 'Missing "pipeline" or "variables" in request body'
        }), 400

    now = datetime.now()
    session_name = f"session_{now.strftime('%Y%m%d')}"
    session_dir = os.path.join(SESSIONS_DIR, session_name)
    log_filename = f"logs_{now.strftime('%Y%m%d_%H%M%S')}.txt"
    log_path = os.path.join(session_dir, log_filename)

    try:
        os.makedirs(session_dir, exist_ok=True)
    except OSError as e:
        return jsonify({
            'status': 'error',
            'message': f'Failed to create session directory: {str(e)}'
        }), 500

    variables_path = os.path.abspath(os.path.join(session_dir, 'variables.toml'))
    try:
        with open(variables_path, 'w', encoding='utf-8') as f:
            f.write(variables_raw)
    except OSError as e:
        return jsonify({
            'status': 'error',
            'message': f'Failed to write variables.toml: {str(e)}'
        }), 500

    if isinstance(pipeline_raw, str):
        try:
            pipeline_obj = json.loads(pipeline_raw)
        except json.JSONDecodeError as e:
            return jsonify({
                'status': 'error',
                'message': f'Invalid JSON in pipeline: {str(e)}'
            }), 400
    else:
        pipeline_obj = pipeline_raw

    pipeline_obj['variables'] = variables_path

    pipeline_path = os.path.abspath(os.path.join(session_dir, 'pipeline.json'))
    try:
        with open(pipeline_path, 'w', encoding='utf-8') as f:
            json.dump(pipeline_obj, f, indent=2, ensure_ascii=False)
    except OSError as e:
        return jsonify({
            'status': 'error',
            'message': f'Failed to write pipeline.json: {str(e)}'
        }), 500

    computing_server_path = os.path.abspath(COMPUTING_SERVER_PATH)
    try:
        log_file = open(log_path, 'w', encoding='utf-8')
        process = subprocess.Popen(
            [computing_server_path, pipeline_path],
            stdout=log_file,
            stderr=subprocess.STDOUT,
            cwd=os.path.dirname(os.path.abspath(__file__))
        )
    except OSError as e:
        log_file.close()
        return jsonify({
            'status': 'error',
            'message': f'Failed to start computing server: {str(e)}'
        }), 500

    return jsonify({
        'status': 'ok',
        'session_id': session_name,
        'pid': process.pid
    })


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=SERVER_PORT, debug=True)
