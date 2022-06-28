"""Function for setting up logging."""
import os
import json
import logging
import logging.config


def setup_logging(default_path=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'logging.json'),
                  default_level=logging.INFO,
                  env_key='LOG_CFG'):
    """Set up logging configuration."""

    path = default_path
    value = os.getenv(env_key, None)

    if value:
        path = value

    if os.path.exists(path):

        with open(path, 'rt') as f:
            config = json.load(f)
        logging.config.dictConfig(config)

        logger = logging.getLogger(__name__)
        logger.info('Loaded logging configuration from %s.', path)

    else:
        logging.basicConfig(level=default_level)

        logger = logging.getLogger(__name__)
        logger.info('Using a basic logging configuration with level %s.', default_level)
