const Redis = require("ioredis");

const redis = new Redis();

const fs = require("fs/promises");

//simplified config loading, in reality this would be pulled externally (S3 bucket, etc.)
async function loadFromConfig() {
    const config = await fs.readFile("./rules.json", "utf8");
    return JSON.parse(config);
}

async function checkCounter(user_id, endpoint, endpoint_max, global_max, windowSeconds) {

    const redisKey = `rate-limit:${user_id}:${endpoint}`;
    const globalKey = `rate-limit:${user_id}:global`;

    const endpoint_count = await redis.incr(redisKey);

    if (endpoint_count == 1) {
        const expire_time = await redis.expire(redisKey, windowSeconds);
        console.log(`Endpoint key expired in ${windowSeconds} seconds`);
    }

    const global_count = await redis.incr(globalKey);

    if (global_count == 1) {
        const global_expire_time = await redis.expire(globalKey, windowSeconds);
        console.log(`Global key expires in ${windowSeconds} seconds`);
    }
    
    const endpointRemaining = endpoint_max - endpoint_count;
    const globalRemaining = global_max - global_count;
    const remaining = Math.max(0, Math.min(endpointRemaining, globalRemaining));


    return {allowed: endpoint_count < endpoint_max && global_count < global_max, endpoint_count, global_count, remaining};


}

let rules = {};

async function getRules() {
    const config = await loadFromConfig();
    rules = config;
    if (Object.keys(rules).length == 0) {
        rules = {default: {endpoint_max: 10, global_max: 100, windowSeconds: 60}};
    }
}

(async () => {
    await getRules();
    setInterval(getRules, 1000);

    app.listen(3000, () => {
        console.log("Server is running on port 3000");
    });
})();


async function rateLimiter(req, res, next) {

    const user_ip = req.ip;
    const user_path = req.path;

    const rule = rules[user_path] || rules.default;

    const {endpoint_max, global_max, windowSeconds} = rule;

    try {

        const {allowed, endpoint_count, global_count, remaining} = await checkCounter(user_ip, user_path, endpoint_max, global_max, windowSeconds);

        res.set("X-RateLimit-Remaining", remaining);

    } catch (error) {
        console.error("Rate limiter error:", error);
        return next();
    }
}